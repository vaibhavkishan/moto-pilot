package com.vaibhavkishan.motopilot.routing

import com.vaibhavkishan.motopilot.offline.GeoBounds
import com.vaibhavkishan.motopilot.offline.OfflineErrorKind
import com.vaibhavkishan.motopilot.offline.OfflineMapException
import com.vaibhavkishan.motopilot.offline.StreetSource
import com.vaibhavkishan.motopilot.offline.StreetWay
import kotlinx.coroutines.*
import okhttp3.*
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** Small sequential batches, full geometries, no partial response accepted as offline coverage. */
class OverpassStreetProvider(
    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS).readTimeout(65, TimeUnit.SECONDS)
        .callTimeout(75, TimeUnit.SECONDS).build()
) : StreetSource {
    override suspend fun fetch(bounds: List<GeoBounds>): List<StreetWay> = withContext(Dispatchers.IO) {
        try {
            require(bounds.size in 1..4)
            val selectors = bounds.joinToString("\n") {
                "way[highway][highway!~\"^(proposed|construction|abandoned|razed)$\"](" +
                    it.south + "," + it.west + "," + it.north + "," + it.east + ");"
            }
            val query = "[out:json][timeout:45][maxsize:67108864];(" + selectors + ");out geom;"
            val request = Request.Builder().url("https://overpass-api.de/api/interpreter")
                .header("User-Agent", "MotoPilot/3.0 (personal route offline map)")
                .post(FormBody.Builder().add("data", query).build()).build()
            val json = requestBody(request)
            currentCoroutineContext().ensureActive()
            val root = JSONObject(json)
            // Overpass may return HTTP 200 with partial results plus a timeout/size remark.
            if (root.optString("remark").isNotBlank()) {
                throw OfflineMapException(OfflineErrorKind.MAP_SERVER, "Map server busy. Try again to finish the remaining areas.")
            }
            val elements = root.getJSONArray("elements")
            var pointCount = 0
            val ways = buildList {
                for (i in 0 until elements.length()) {
                    currentCoroutineContext().ensureActive()
                    val element = elements.getJSONObject(i)
                    if (element.optString("type") != "way") continue
                    val geometry = element.optJSONArray("geometry") ?: continue
                    require(geometry.length() >= 2) { "Incomplete street geometry" }
                    pointCount += geometry.length()
                    if (pointCount > 300000) {
                        throw OfflineMapException(OfflineErrorKind.TOO_MUCH_DATA, "Area has too much data. Try a smaller scale.")
                    }
                    val points = List(geometry.length()) { g ->
                        val point = geometry.getJSONObject(g)
                        LatLon(point.getDouble("lat"), point.getDouble("lon")).also {
                            require(it.lat in -90.0..90.0 && it.lon in -180.0..180.0)
                        }
                    }
                    add(StreetWay(element.getLong("id"), points))
                }
            }
            ways
        } catch (e: CancellationException) { throw e }
        catch (e: Exception) { throw e }
    }

    private suspend fun requestBody(request: Request): String = suspendCancellableCoroutine { continuation ->
        val call = client.newCall(request)
        continuation.invokeOnCancellation { call.cancel() }
        call.enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                if (continuation.isActive) continuation.resumeWithException(e)
            }
            override fun onResponse(call: Call, response: Response) {
                try {
                    val body = response.use {
                        if (!it.isSuccessful) {
                            throw OfflineMapException(OfflineErrorKind.MAP_SERVER, "Map server error (HTTP " + it.code + "). Wait and try again.")
                        }
                        val stream = it.body?.byteStream() ?: error("Empty map response")
                        val output = ByteArrayOutputStream()
                        val buffer = ByteArray(8192)
                        while (true) {
                            val count = stream.read(buffer)
                            if (count < 0) break
                            if (output.size() + count > 20 * 1024 * 1024) {
                                throw OfflineMapException(OfflineErrorKind.TOO_MUCH_DATA, "Map response too large")
                            }
                            output.write(buffer, 0, count)
                        }
                        output.toString("UTF-8")
                    }
                    if (continuation.isActive) continuation.resume(body)
                } catch (e: Exception) {
                    if (continuation.isActive) continuation.resumeWithException(e)
                }
            }
        })
    }
}
