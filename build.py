"""Builds the single offline file GLUT-Code-Builder.html from ui.html,
gen.js, presets.js, strip.js and master/part*.cpp (so it can be copied to
a pen drive and opened in any browser without internet).

Also writes master.cpp (all parts joined) for reference."""
import json
import pathlib

here = pathlib.Path(__file__).parent


def read(name):
    return (here / name).read_text(encoding="utf-8-sig")


html = read("ui.html")
master = "\n".join(read("master/" + p + ".cpp") for p in ["part1", "part2", "part3", "part4", "part5"])
(here / "master.cpp").write_text(master, encoding="utf-8")

blocks = {
    "/*GEN*/": read("gen.js"),
    "/*PRESETS*/": read("presets.js"),
    "/*STRIP*/": read("strip.js"),
    "/*MASTER*/": "window.APP_MASTER = " + json.dumps(master).replace("</", "<\\/") + ";",
}
for marker, code in blocks.items():
    tag = "<script>" + marker + "</script>"
    assert tag in html, "missing " + tag
    html = html.replace(tag, "<script>\n" + code + "\n</script>")

for name in ("GLUT-Code-Builder.html", "index.html"):   # index.html = GitHub Pages entry
    (here / name).write_text(html, encoding="utf-8")
print("wrote GLUT-Code-Builder.html and index.html,", len(html), "bytes")
