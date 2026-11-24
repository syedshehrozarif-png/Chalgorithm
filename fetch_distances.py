#!/usr/bin/env python3
"""fetch_distances.py - run this ONCE to get real road distances for Chalgorithm from OpenRouteService (ORS).

  reads : nodes.txt, edges.txt   (made by the map editor)
          areas.csv              (optional: coordinates you checked or typed yourself)
  writes: areas.csv              (where each area was found, so you can review it)
          weights.txt            (real road km for every road, in both directions)

The game itself never uses the internet: it only reads weights.txt.   Python 3, standard library only.

  python fetch_distances.py --review-only    find the areas, write areas.csv, stop (check it on a map!)
  python fetch_distances.py                  get the distances and write weights.txt

API key: put it in a file called ors_key.txt (one line), or set the ORS_API_KEY environment variable.
Never share or submit ors_key.txt.  Free key: openrouteservice.org -> sign up -> Dashboard -> Request a token.
"""
import csv, json, math, os, sys, time
import urllib.request, urllib.error, urllib.parse

# ---------- settings (like #define in C++) ----------
BASE = "https://api.openrouteservice.org"
CITY, COUNTRY = "Karachi, Pakistan", "PK"       # added to every area name when searching
FOCUS_LON, FOCUS_LAT = 67.0011, 24.8607         # search results are biased towards this point
CHUNK = 12                                      # areas asked about per matrix request
PAUSE_MATRIX = 1.6                              # seconds between matrix requests (free plan: 40 per minute)
PAUSE_GEOCODE = 0.7                             # seconds between name lookups
NO_KEY = ("No API key found. Put your key in a file called ors_key.txt (one line), or set ORS_API_KEY.\n"
          "Free key: openrouteservice.org -> sign up -> Dashboard -> Request a token. Do not share or submit that file.")


# ---------- map files ----------
def fnv1a(data):
    """Same fingerprint as Graph.h: 32-bit FNV-1a over the bytes, ignoring '\\r'."""
    h = 2166136261
    for c in data.replace(b"\r", b""):
        h ^= c
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def read_map():
    """Returns nodes = [(name, x, y), ...], edges = [(a, b), ...] and the fingerprint of the two files."""
    for f in ("nodes.txt", "edges.txt"):
        if not os.path.exists(f):
            sys.exit("%s not found. Run the map editor and press S to save your map first." % f)
    raw_nodes = open("nodes.txt", "rb").read()
    raw_edges = open("edges.txt", "rb").read()
    nodes, edges = [], []
    for line in raw_nodes.decode("utf-8").splitlines():
        p = line.split()                                        # split on spaces, like reading with >>
        if len(p) >= 3:
            nodes.append((p[0].replace("_", " "), float(p[1]), float(p[2])))
    for line in raw_edges.decode("utf-8").splitlines():
        p = line.split()
        if len(p) >= 2:
            a, b = int(p[0]), int(p[1])
            if 0 <= a < len(nodes) and 0 <= b < len(nodes) and a != b:
                edges.append((a, b))
    if not nodes or not edges:
        sys.exit("The map has no areas or no roads yet.")
    return nodes, edges, fnv1a(raw_nodes + raw_edges)


# ---------- talking to ORS ----------
def get_key():
    if os.environ.get("ORS_API_KEY"):
        return os.environ["ORS_API_KEY"].strip()
    if os.path.exists("ors_key.txt"):
        return open("ors_key.txt").read().strip()
    sys.exit(NO_KEY)


def call(url, body=None):
    """Sends one request (a GET, or a POST if there is a body) and returns the answer as Python data.
    If ORS says 'too many requests' (429) it waits a minute and tries again, up to 3 times in total."""
    headers = {"Authorization": get_key(), "Accept": "application/json, application/geo+json"}
    data = None
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, data=data, headers=headers)
            with urllib.request.urlopen(request, timeout=60) as reply:
                return json.loads(reply.read().decode("utf-8"))
        except urllib.error.HTTPError as e:                     # the server answered with an error code
            if e.code == 429 and attempt < 2:
                print("  rate limit reached, waiting 60 s ...")
                time.sleep(60)
                continue
            text = e.read().decode("utf-8", "replace")
            if e.code in (401, 403):
                text += "  (check your API key)"
            sys.exit("ORS refused the request: HTTP %s: %s" % (e.code, text))
        except urllib.error.URLError as e:                      # no answer at all (no internet?)
            sys.exit("Could not reach %s: %s" % (url.split("?")[0], e.reason))


def geocode(name):
    """Looks an area name up. Returns (lat, lon, label) or None if ORS knows no such place."""
    query = urllib.parse.urlencode({"text": "%s, %s" % (name, CITY), "boundary.country": COUNTRY,
                                    "focus.point.lon": FOCUS_LON, "focus.point.lat": FOCUS_LAT, "size": 1})
    features = call(BASE + "/geocode/search?" + query).get("features") or []
    if not features:
        return None
    lon, lat = features[0]["geometry"]["coordinates"]
    return lat, lon, features[0].get("properties", {}).get("label", "")


# ---------- areas.csv: where is each area on earth? ----------
def find_areas(nodes):
    """Returns one (lat, lon, label, source) per area, in the same order as nodes. Rows already in areas.csv are
    reused (so your manual fixes survive); only missing areas are looked up. Then areas.csv is written again."""
    known = {}                                                  # name -> row, like a map<string, row> in C++
    if os.path.exists("areas.csv"):
        with open("areas.csv", newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                try:
                    if row.get("source") == "mock":
                        continue                                # made-up coordinates from an old test run: never reuse
                    known[row["name"]] = (float(row["lat"]), float(row["lon"]), row.get("label", ""), row.get("source", "csv"))
                except (ValueError, KeyError, TypeError):
                    pass                                        # a damaged row is simply ignored
    areas = []
    for name, x, y in nodes:
        if name in known:
            areas.append(known[name])
            continue
        get_key()                                               # stop here if there is no key
        print("  looking up %-20s" % name, end=" ", flush=True)
        hit = geocode(name)
        if hit is None:
            sys.exit("\n'%s' was not found. Add a row for it to areas.csv (name,lat,lon,label,source) and run again." % name)
        print("->", hit[2])
        areas.append((hit[0], hit[1], hit[2], "geocoded"))
        time.sleep(PAUSE_GEOCODE)
    with open("areas.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["name", "lat", "lon", "label", "source"])
        for i in range(len(nodes)):
            w.writerow([nodes[i][0], "%.6f" % areas[i][0], "%.6f" % areas[i][1], areas[i][2], areas[i][3]])
    return areas


# ---------- the distance table ----------
def fetch_chunk(sources, areas, neighbours, distances):
    """One ORS request: the driving distance from every area in 'sources' to its neighbouring areas.
    Results are stored in 'distances' as distances[(from, to)] = km. Returns how many pairs were asked for."""
    dests = []                                                  # every neighbour of every source, once each, sorted
    for s in sources:
        for d in neighbours[s]:
            if d not in dests:
                dests.append(d)
    dests.sort()
    used = sorted(set(sources) | set(dests))                    # all areas mentioned in this request
    where = {}                                                  # area number -> position in 'used'
    for i in range(len(used)):
        where[used[i]] = i
    body = {"locations": [[areas[n][1], areas[n][0]] for n in used],        # ORS wants [lon, lat]
            "sources": [where[s] for s in sources], "destinations": [where[d] for d in dests],
            "metrics": ["distance"], "units": "km"}
    table = call(BASE + "/v2/matrix/driving-car", body)["distances"]        # table[i][j] = km from sources[i] to dests[j]
    time.sleep(PAUSE_MATRIX)
    for i in range(len(sources)):
        for j in range(len(dests)):
            if dests[j] in neighbours[sources[i]]:              # keep only real roads, not every pair
                distances[(sources[i], dests[j])] = table[i][j]
    return len(sources) * len(dests)


def main():
    for arg in sys.argv[1:]:
        if arg != "--review-only":
            sys.exit("Unknown option %s\nUsage: python fetch_distances.py [--review-only]" % arg)

    nodes, edges, fingerprint = read_map()
    print("Map: %d areas, %d roads (fingerprint %d)" % (len(nodes), len(edges), fingerprint))

    print("Step 1: find the areas")
    areas = find_areas(nodes)
    if "--review-only" in sys.argv:
        print("\nareas.csv written. Open each row on a map, fix any wrong lat/lon (set source to 'manual'), then run again without --review-only.")
        return

    neighbours = {}                                             # area number -> set of directly connected areas
    for i in range(len(nodes)):
        neighbours[i] = set()
    for a, b in edges:
        neighbours[a].add(b)
        neighbours[b].add(a)
    connected = [i for i in range(len(nodes)) if neighbours[i]]                 # areas that have at least one road
    chunks = [connected[i:i + CHUNK] for i in range(0, len(connected), CHUNK)]  # split into groups of CHUNK
    print("Step 2: distances for %d roads (both directions) in about %d request(s)" % (len(edges), len(chunks)))

    distances, pairs = {}, 0
    for c in chunks:
        pairs += fetch_chunk(c, areas, neighbours, distances)
    print("  %d request(s), %d pairs asked for" % (len(chunks), pairs))

    # ---- weights.txt: one line per road in edges.txt (the game's Dijkstra routes through in-between areas) ----
    lines, rows, bad = ["map %d" % fingerprint], [], []
    for a, b in edges:
        ab, ba = distances.get((a, b)), distances.get((b, a))
        px = math.hypot(nodes[a][1] - nodes[b][1], nodes[a][2] - nodes[b][2]) / 40.0     # length on the map picture
        if not ab or not ba or ab <= 0 or ba <= 0:              # missing, null or zero: keep the picture estimate
            bad.append((a, b))
            continue
        lines.append("%d %d %.3f %.3f" % (a, b, ab, ba))
        rows.append((a, b, px, ab, ba))
    with open("weights.txt", "w") as f:
        f.write("\n".join(lines) + "\n")

    # ---- report: one line per road, plus CHECK lines for anything that looks wrong ----
    print("\n%-34s %7s %9s %9s %6s" % ("road", "picture", "real a>b", "real b>a", "ratio"))
    ratios = sorted(((ab + ba) / 2) / px for (_, _, px, ab, ba) in rows if px > 0.01)
    median = ratios[len(ratios) // 2] if ratios else 1.0
    warnings = []
    for a, b, px, ab, ba in rows:
        r = ((ab + ba) / 2) / px if px > 0.01 else 0
        print("%-34s %7.1f %9.1f %9.1f %6.2f" % (nodes[a][0] + " - " + nodes[b][0], px, ab, ba, r))
        if r < 0.6 * median or r > 1.7 * median:
            warnings.append("%s - %s: real/picture ratio %.2f, typical %.2f. Is one of the two areas placed or looked up wrongly?" % (nodes[a][0], nodes[b][0], r, median))
        if abs(ab - ba) > 0.25 * min(ab, ba):
            warnings.append("%s - %s: %.1f km one way but %.1f km back (one-way streets?)." % (nodes[a][0], nodes[b][0], ab, ba))
    for a, b in bad:
        warnings.append("%s - %s: no road distance (an area may be off the road network). That road keeps its picture estimate." % (nodes[a][0], nodes[b][0]))
    if rows:
        smallest = min(min(ab, ba) / px for (_, _, px, ab, ba) in rows if px > 0.01)
        print("\nSummary: %d of %d roads have real distances | median real/picture %.2f | smallest %.2f (A* scales its guess by this)"
              % (len(rows), len(edges), median, smallest))
    for w in warnings:
        print("CHECK:", w)
    print("\nweights.txt written. Start the game or the editor and it is used automatically.")


if __name__ == "__main__":
    main()
