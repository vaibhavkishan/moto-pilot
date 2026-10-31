// Renders every CYD screen with the real firmware drawing code, using the host TFT stub.
// Usage: screens_preview <output dir>   (then run screens_preview.py on the same folder)
#include <TFT_eSPI.h>
#include <stdio.h>
#include <string.h>

#include "map_renderer.h"
#include "maps_theme.h"
#include "scene.h"

namespace {

struct Scenario {
  const char* name;
  uint8_t maneuver;
  int distanceM;
  const char* instruction;
  const char* street;
  int remainingM;
  int remainingS;
  const char* eta;
  bool offRoute;
  bool rerouting;
  bool gpsWeak;
  bool destVisible;
  uint8_t style = STYLE_CLASSIC;
  float progress = -1.0f;
  float zoom = 1.0f;
};

// Canvas is 408x408 with the user at (204, 204); the CYD rotates it heading-up.
NavState buildNav(const Scenario& s) {
  NavState n;
  n.valid = true;
  n.style = s.style;
  n.tripProgress = s.progress;
  n.headingUpMode = true;
  n.bearingValid = true;
  n.distanceM = s.distanceM;
  n.maneuver = s.maneuver;
  n.remainingM = s.remainingM;
  n.remainingS = s.remainingS;
  n.offRoute = s.offRoute;
  n.rerouting = s.rerouting;
  n.gpsWeak = s.gpsWeak;
  strncpy(n.eta, s.eta, sizeof(n.eta) - 1);
  strncpy(n.instruction, s.instruction, sizeof(n.instruction) - 1);
  strncpy(n.street, s.street, sizeof(n.street) - 1);

  int count = 0;
  for (int y = 40; y <= 380; y += 56) {
    n.streetX0[count] = 10; n.streetY0[count] = y; n.streetX1[count] = 398; n.streetY1[count] = y; count++;
  }
  for (int x = 40; x <= 380; x += 64) {
    n.streetX0[count] = x; n.streetY0[count] = 10; n.streetX1[count] = x; n.streetY1[count] = 398; count++;
  }
  n.streetSegmentCount = count;

  static const int16_t ahead[][2] = {{204, 204}, {204, 150}, {204, 110}, {250, 110}, {250, 80}};
  n.routeCount = 5;
  for (int i = 0; i < 5; i++) { n.routeX[i] = ahead[i][0]; n.routeY[i] = ahead[i][1]; }
  static const int16_t done[][2] = {{204, 380}, {204, 320}, {204, 260}, {204, 204}};
  n.routeDoneCount = 4;
  for (int i = 0; i < 4; i++) { n.routeDoneX[i] = done[i][0]; n.routeDoneY[i] = done[i][1]; }
  if (s.destVisible) {
    n.hasDest = true;
    n.destX = 250;
    n.destY = 80;
  }
  n.userX = 204;
  n.userY = 204;
  n.hasUserPosition = true;
  return n;
}

void save(const TFT_eSPI& tft, const char* dir, const char* name, int dark) {
  char ppm[600], txt[600];
  snprintf(ppm, sizeof ppm, "%s/%s_%s.ppm", dir, name, dark ? "dark" : "light");
  snprintf(txt, sizeof txt, "%s/%s_%s.txt", dir, name, dark ? "dark" : "light");
  tft.dump(ppm, txt);
}

}  // namespace

int main(int argc, char** argv) {
  const char* dir = argc > 1 ? argv[1] : ".";
  const Scenario scenarios[] = {
      {"nav_turn", MAN_RIGHT, 350, "Turn right", "Rua Augusta", 4200, 840, "18:42", false, false, false, true},
      {"nav_turn_zoom", MAN_RIGHT, 45, "Turn right", "Rua Augusta", 4200, 840, "18:42", false, false, false, true,
       STYLE_CLASSIC, -1.0f, 2.8f},
      {"nav_rerouting", MAN_NONE, 0, "Return to the route", "", -1, -1, "", true, true, false, false},
      {"nav_weak_gps", MAN_STRAIGHT, 120, "Continue straight", "Avenida Paulista", 2600, 540, "18:35", false, false, true, true},
      {"nav_arriving", MAN_ARRIVE, 40, "Destination ahead", "Rua Oscar Freire", 40, 30, "18:27", false, false, false, true},
      {"minimal_turn", MAN_RIGHT, 300, "Turn right", "Rua Augusta", 4200, 840, "18:42", false, false, false, true, STYLE_MINIMAL, 0.32f},
      {"minimal_km", MAN_STRAIGHT, 1840, "Continue straight", "", 9000, 1200, "18:50", false, false, false, false, STYLE_MINIMAL, 0.6f},
      {"minimal_rerouting", MAN_NONE, 0, "Return to the route", "", -1, -1, "", true, true, false, false, STYLE_MINIMAL, 0.4f},
      {"minimal_arriving", MAN_ARRIVE, 40, "Destination ahead", "", 40, 30, "18:27", false, false, false, true, STYLE_MINIMAL, 0.97f},
  };

  for (int dark = 0; dark < 2; dark++) {
    mapsThemeSet(dark ? MAPS_THEME_DARK : MAPS_THEME_LIGHT);
    {
      TFT_eSPI tft;
      drawStatusScreen(tft, "MotoPilot", "Starting BLE...", nullptr);
      save(tft, dir, "boot", dark);
    }
    {
      TFT_eSPI tft;
      drawHomeScreen(tft, dark, false, "MotoPilot-CYD");
      save(tft, dir, "home", dark);
    }
    {
      TFT_eSPI tft;
      drawLoadingScreen(tft, dark);
      drawLoadingBar(tft, dark, 450, true);
      save(tft, dir, "loading", dark);
    }
    for (const Scenario& s : scenarios) {
      TFT_eSPI tft;
      MapRenderer renderer(tft);
      renderer.begin();
      MapView view;
      view.headingKnown = true;
      view.headingDeg = 0.0f;
      view.zoom = s.zoom;
      renderer.draw(buildNav(s), view);
      save(tft, dir, s.name, dark);
    }
    {
      TFT_eSPI tft;
      drawArrivalScreen(tft, dark);
      save(tft, dir, "arrival", dark);
    }
    {
      TFT_eSPI tft;
      drawArrivalScreenMinimal(tft, dark);
      save(tft, dir, "minimal_arrival", dark);
    }
  }
  return 0;
}
