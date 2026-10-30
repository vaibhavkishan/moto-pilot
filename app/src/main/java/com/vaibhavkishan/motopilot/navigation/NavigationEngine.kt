package com.vaibhavkishan.motopilot.navigation

import com.vaibhavkishan.motopilot.map.MapProjector
import com.vaibhavkishan.motopilot.map.StreetContextProjector
import com.vaibhavkishan.motopilot.maps.MapsHtmlHolder
import com.vaibhavkishan.motopilot.models.AppSettings
import com.vaibhavkishan.motopilot.network.BleNavClient
import com.vaibhavkishan.motopilot.protocol.NavMessage
import com.vaibhavkishan.motopilot.routing.LatLon
import com.vaibhavkishan.motopilot.offline.OfflineMapRepository
import com.vaibhavkishan.motopilot.routing.OsrmRouteProvider
import com.vaibhavkishan.motopilot.routing.RouteData
import com.vaibhavkishan.motopilot.routing.RouteProfile
import com.vaibhavkishan.motopilot.routing.RouteProgressCalculator
import com.vaibhavkishan.motopilot.routing.RouteSplit
import com.vaibhavkishan.motopilot.routing.EtaFormatter
import com.vaibhavkishan.motopilot.routing.ManeuverIcon
import com.vaibhavkishan.motopilot.utils.StatsTracker
import com.vaibhavkishan.motopilot.utils.TransmissionHub
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.flow.conflate
import kotlinx.coroutines.delay
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import android.os.SystemClock
import android.util.Log
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

class NavigationEngine(
    private val scope: CoroutineScope,
    private val locationTracker: LocationTracker,
    private val routeProvider: OsrmRouteProvider,
    private val offlineMap: OfflineMapRepository,
    private val navClient: BleNavClient,
    private val settingsProvider: suspend () -> AppSettings,
    private val onNavigationEnded: () -> Unit = {}
) {

    private var route: RouteData? = null
    private var progressCalculator: RouteProgressCalculator? = null
    private var destination: LatLon? = null
    private var locationJob: Job? = null
    private var downloadJob: Job? = null
    private var activeSettings: AppSettings? = null
    private var gpsFilter: GpsFilter? = null
    private val headingGate = HeadingGate()
    private val rerouteGate = RerouteGate()
    private var rerouteJob: Job? = null
    // Written by the reroute job, applied by publishUpdate so route and calculator change together.
    @Volatile private var pendingReroute: RouteData? = null
    @Volatile private var rerouting = false
    @Volatile private var arrived = false
    private val arrivalDetector = ArrivalDetector()
    private var arrivalJob: Job? = null
    private var gpsWatchdog: Job? = null
    private var lastAcceptedMs = 0L
    @Volatile private var lastMessage: NavMessage? = null
    private val statsTracker = StatsTracker()

    fun startNavigation(destLat: Double, destLon: Double) {
        stopNavigation()
        MapsHtmlHolder.clear()
        TransmissionHub.clearNavigationError()
        destination = LatLon(destLat, destLon)
        TransmissionHub.setNavigating(true)
        headingGate.reset()
        rerouteGate.reset()
        locationJob = scope.launch {
            try {
                val settings = settingsProvider()
                activeSettings = settings
                gpsFilter = GpsFilter(when (settings.routeProfile) {
                    com.vaibhavkishan.motopilot.routing.RouteProfile.WALKING -> 8.0
                    com.vaibhavkishan.motopilot.routing.RouteProfile.BIKE -> 25.0
                    else -> 70.0
                })
                val intervalMs = 1000L / settings.navUpdateHz.coerceIn(1, 5)
                if (navClient.connectionState.value == com.vaibhavkishan.motopilot.models.ConnectionState.CONNECTED) {
                    navClient.sendNavMessage(NavMessage.loading())
                }
                val originUpdate = withTimeout(45_000) { locationTracker.locationFlow(intervalMs).first { acceptUpdate(it) } }
                val origin = LatLon(originUpdate.lat, originUpdate.lon)
                val dest = destination ?: return@launch
                // Reopening the same trip near its route works without another routing request.
                val saved = offlineMap.findRoute(origin, dest, settings.routeProfile)
                val selected = saved ?: routeProvider.fetchRoute(origin, dest, settings.routeProfile).getOrElse {
                    error("Unable to calculate the route. Check your internet connection and the destination.")
                }
                offlineMap.saveRoute(selected, dest, settings.routeProfile)
                route = selected
                progressCalculator = RouteProgressCalculator(selected)
                retryOfflineDownload()
                gpsWatchdog = scope.launch {
                    while (true) {
                        delay(1500)
                        if (SystemClock.elapsedRealtime() - lastAcceptedMs > 5000) {
                            TransmissionHub.updateGps(TransmissionHub.gps.value.copy(weak = true))
                            sendWeakGpsFrame()
                        }
                    }
                }
                if (SystemClock.elapsedRealtime() - originUpdate.elapsedMs < 3000) publishUpdate(originUpdate, settings)
                locationTracker.locationFlow(intervalMs).conflate().collect {
                    if (acceptUpdate(it)) publishUpdate(it, settings) else if (TransmissionHub.gps.value.weak) sendWeakGpsFrame()
                }
            } catch (e: kotlinx.coroutines.TimeoutCancellationException) {
                downloadJob?.cancel()
                gpsWatchdog?.cancel()
                TransmissionHub.setNavigating(false)
                TransmissionHub.navigationFailed("No GPS fix. Move to an open area and try again.")
                onNavigationEnded()
            } catch (e: CancellationException) { throw e }
            catch (e: Exception) {
                downloadJob?.cancel()
                gpsWatchdog?.cancel()
                TransmissionHub.setNavigating(false)
                TransmissionHub.navigationFailed("Unable to start navigation. Check precise location, internet and the destination.")
                onNavigationEnded()
            }
        }
    }

    fun retryOfflineDownload() {
        if (downloadJob?.isActive == true) return
        val currentRoute = route ?: return
        val settings = activeSettings ?: return
        downloadJob = scope.launch {
            offlineMap.prepare(currentRoute, settings.mapScaleMeters) { progress ->
                withContext(Dispatchers.Main) { TransmissionHub.updateOfflineMap(progress) }
            }
        }
    }

    fun stopNavigation() {
        locationJob?.cancel()
        locationJob = null
        downloadJob?.cancel()
        downloadJob = null
        gpsWatchdog?.cancel()
        gpsWatchdog = null
        rerouteJob?.cancel()
        rerouteJob = null
        arrivalJob?.cancel()
        arrivalJob = null
        arrived = false
        arrivalDetector.reset()
        pendingReroute = null
        rerouting = false
        gpsFilter = null
        lastMessage = null
        lastAcceptedMs = 0
        route = null
        progressCalculator = null
        destination = null
        activeSettings = null
        TransmissionHub.setNavigating(false)
        statsTracker.reset()
    }

    private fun acceptUpdate(update: LocationUpdate): Boolean {
        val accepted = gpsFilter?.accept(GpsSample(LatLon(update.lat, update.lon), update.accuracyM,
            update.speedMps, update.bearing, update.elapsedMs), SystemClock.elapsedRealtime()) == true
        if (accepted) lastAcceptedMs = update.elapsedMs
        if (accepted || update.elapsedMs > lastAcceptedMs || SystemClock.elapsedRealtime() - lastAcceptedMs > 5000) {
            TransmissionHub.updateGps(GpsStatus(update.accuracyM.takeIf { it.isFinite() }, lastAcceptedMs, !accepted))
        }
        Log.d("MotoPilotGPS", "accepted=$accepted accuracy=${update.accuracyM} ageMs=${SystemClock.elapsedRealtime() - update.elapsedMs}")
        return accepted
    }

    private fun sendWeakGpsFrame() {
        if (arrived) return
        val previous = lastMessage ?: return
        if (previous.gpsWeak) return
        previous.copy(gpsWeak = true, bearingValid = false).also { lastMessage = it; navClient.sendNavMessage(it.toJson()) }
    }

    private suspend fun publishUpdate(raw: LocationUpdate, settings: AppSettings) = withContext(Dispatchers.Default) {
        if (arrived) return@withContext
        val buildStarted = SystemClock.elapsedRealtime()
        pendingReroute?.let { newRoute ->
            pendingReroute = null
            route = newRoute
            progressCalculator = RouteProgressCalculator(newRoute)
            downloadJob?.cancel()
            downloadJob = null
            retryOfflineDownload()
        }
        val currentRoute = route ?: return@withContext
        val progress = progressCalculator?.update(LatLon(raw.lat, raw.lon))
            ?: com.vaibhavkishan.motopilot.routing.TripProgress()
        val position = GpsFilter.displayPosition(LatLon(raw.lat, raw.lon), progress.matchedPosition,
            progress.distanceFromRouteM, raw.accuracyM, raw.speedMps, raw.bearing, progress.routeBearing)
        val update = raw.copy(lat = position.lat, lon = position.lon)
        TransmissionHub.updateTripProgress(progress)
        val deviation = progress.distanceFromRouteM
        val offRoute = progress.offRoute || (deviation != null && deviation > rerouteDistanceM(settings.routeProfile, raw.accuracyM))
        if (arrivalDetector.update(progress.remainingDistanceM, offRoute, ArrivalDetector.thresholdM(settings.routeProfile))) {
            handleArrival()
            return@withContext
        }
        if (rerouteGate.shouldStart(offRoute, SystemClock.elapsedRealtime())) {
            startReroute(LatLon(raw.lat, raw.lon), settings)
        }
        if (navClient.connectionState.value != com.vaibhavkishan.motopilot.models.ConnectionState.CONNECTED) {
            return@withContext
        }

        val step = progress.nextStepIndex?.let { currentRoute.steps.getOrNull(it) }
        val distanceM = progress.distanceToNextStepM ?: 0
        val simplified = MapProjector.simplifyRoute(
            currentRoute.coordinates.map { it.lat to it.lon }
        )
        val parts = RouteSplit.split(currentRoute.coordinates, progress.segmentIndex, progress.matchedPosition)
        val screenRoute = MapProjector.projectRoute(
            centerLat = update.lat,
            centerLon = update.lon,
            routePoints = parts.ahead.map { it.lat to it.lon },
            scaleMeters = settings.mapScaleMeters
        )
        val screenDone = if (parts.done.size >= 2) MapProjector.projectRoute(
            centerLat = update.lat,
            centerLon = update.lon,
            routePoints = parts.done.map { it.lat to it.lon },
            scaleMeters = settings.mapScaleMeters,
            maxPoints = 32
        ) else emptyList()
        val destPoint = destination?.let { d ->
            val p = MapProjector.projectUnclipped(update.lat, update.lon, d.lat, d.lon, settings.mapScaleMeters)
            val size = MapProjector.CANVAS_SIZE
            if (p.x >= 0 && p.x < size && p.y >= 0 && p.y < size) p.x.roundToInt() to p.y.roundToInt() else null
        }
        val userPoint = MapProjector.projectPoint(
            centerLat = update.lat,
            centerLon = update.lon,
            lat = update.lat,
            lon = update.lon,
            scaleMeters = settings.mapScaleMeters
        )

        val (streets, covered) = offlineMap.visibleWays(LatLon(update.lat, update.lon), settings.mapScaleMeters)
        TransmissionHub.updateMapCoverage(covered)
        val streetSegments = withContext(Dispatchers.Default) { StreetContextProjector.projectSegments(
            centerLat = update.lat,
            centerLon = update.lon,
            ways = streets,
            scaleMeters = settings.mapScaleMeters
        ) }

        val bearing = if (update.bearing.isFinite()) {
            update.bearing
        } else {
            computeBearingFromRoute(update, simplified)
        }

        val message = NavMessage(
            lat = update.lat,
            lon = update.lon,
            bearing = bearing,
            instruction = step?.instruction ?: if (offRoute) "Return to the route" else "Continue",
            distanceM = distanceM,
            remainingDistanceM = progress.remainingDistanceM,
            remainingDurationS = progress.remainingDurationS,
            offRoute = offRoute,
            speedMps = update.speedMps,
            bearingValid = headingGate.isValid(update.bearing, update.speedMps, update.bearingAccuracyDeg),
            maneuver = step?.let { ManeuverIcon.fromInstruction(it.instruction) } ?: "",
            eta = EtaFormatter.format(System.currentTimeMillis(), progress.remainingDurationS),
            rerouting = rerouting,
            street = step?.street ?: "",
            routeScreenPoints = screenRoute,
            routeDonePoints = screenDone,
            destScreenPoint = destPoint,
            streetSegments = streetSegments,
            userScreenPoint = userPoint,
            html = MapsHtmlHolder.get()
        )

        val json = message.toJson()
        currentCoroutineContext().ensureActive()
        Log.d("MotoPilotMap", "buildMs=${SystemClock.elapsedRealtime() - buildStarted} segments=${streetSegments.size} bytes=${json.toByteArray().size}")
        if (SystemClock.elapsedRealtime() - raw.elapsedMs > 3000) {
            Log.d("MotoPilotMap", "Discarded stale map frame")
            return@withContext
        }
        lastMessage = message
        if (navClient.sendNavMessage(json)) {
            statsTracker.onNavUpdate(json.toByteArray().size)
            TransmissionHub.updateStats(
                statsTracker.currentStats(settings.resolutionLabel)
            )
        }
    }

    private fun handleArrival() {
        arrived = true
        TransmissionHub.announceArrival()
        if (navClient.connectionState.value == com.vaibhavkishan.motopilot.models.ConnectionState.CONNECTED) {
            navClient.sendNavMessage(NavMessage.arrived())
        }
        // Keep GPS and the service alive briefly so the CYD receives the message before everything stops.
        arrivalJob = scope.launch {
            delay(3000)
            stopNavigation()
            onNavigationEnded()
        }
    }

    private fun rerouteDistanceM(profile: RouteProfile, accuracyM: Float): Double {
        val base = when (profile) {
            RouteProfile.WALKING -> 30.0
            RouteProfile.BIKE -> 40.0
            else -> 60.0
        }
        return maxOf(base, if (accuracyM.isFinite()) 2.0 * accuracyM else 0.0)
    }

    private fun startReroute(origin: LatLon, settings: AppSettings) {
        val dest = destination
        if (dest == null) {
            rerouteGate.onFailure(SystemClock.elapsedRealtime())
            return
        }
        rerouting = true
        rerouteJob = scope.launch {
            try {
                val result = routeProvider.fetchRoute(origin, dest, settings.routeProfile)
                val newRoute = result.getOrNull()
                if (newRoute != null) {
                    offlineMap.saveRoute(newRoute, dest, settings.routeProfile)
                    pendingReroute = newRoute
                    rerouteGate.onSuccess()
                } else {
                    rerouteGate.onFailure(SystemClock.elapsedRealtime())
                }
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                rerouteGate.onFailure(SystemClock.elapsedRealtime())
            } finally {
                rerouting = false
            }
        }
    }

    private fun computeBearingFromRoute(
        update: LocationUpdate,
        routePoints: List<Pair<Double, Double>>
    ): Float {
        if (routePoints.size < 2) return update.bearing.takeIf { it.isFinite() } ?: 0f
        var bestIndex = 0
        var minDist = Double.MAX_VALUE
        routePoints.forEachIndexed { index, (lat, lon) ->
            val d = haversineMeters(update.lat, update.lon, lat, lon)
            if (d < minDist) {
                minDist = d
                bestIndex = index
            }
        }
        val nextIndex = (bestIndex + 1).coerceAtMost(routePoints.lastIndex)
        val (lat1, lon1) = routePoints[bestIndex]
        val (lat2, lon2) = routePoints[nextIndex]
        return bearingDegrees(lat1, lon1, lat2, lon2)
    }

    private fun haversineMeters(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val r = 6_371_000.0
        val dLat = Math.toRadians(lat2 - lat1)
        val dLon = Math.toRadians(lon2 - lon1)
        val a = sin(dLat / 2) * sin(dLat / 2) +
            cos(Math.toRadians(lat1)) * cos(Math.toRadians(lat2)) *
            sin(dLon / 2) * sin(dLon / 2)
        return r * 2 * atan2(sqrt(a), sqrt(1 - a))
    }

    private fun bearingDegrees(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Float {
        val dLon = Math.toRadians(lon2 - lon1)
        val y = sin(dLon) * cos(Math.toRadians(lat2))
        val x = cos(Math.toRadians(lat1)) * sin(Math.toRadians(lat2)) -
            sin(Math.toRadians(lat1)) * cos(Math.toRadians(lat2)) * cos(dLon)
        return ((Math.toDegrees(atan2(y, x)) + 360) % 360).toFloat()
    }
}
