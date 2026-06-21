"""Patch portfolio.docx:
   - Insert a "Hinweis" disclaimer right after the title
   - Insert a "2.X Netzwerk-Architektur" section near the end of part 2,
     containing the rendered architecture PNG and a short summary.

The script does NOT touch existing content; it only inserts new paragraphs.
"""
import re, shutil, zipfile, sys, os, struct, io
from pathlib import Path

SRC      = Path(r"C:\Users\valen\Downloads\portfolio.docx")
OUT      = Path(r"C:\Users\valen\Downloads\portfolio_updated.docx")
IMG      = Path(r"C:\Users\valen\Downloads\Bugmin_Networking_Architecture.png")

def png_size(p: Path):
    """Return (width_px, height_px) of a PNG file."""
    with open(p, "rb") as f:
        sig = f.read(8)
        assert sig == b"\x89PNG\r\n\x1a\n"
        # IHDR chunk
        f.read(4)  # length
        assert f.read(4) == b"IHDR"
        w = struct.unpack(">I", f.read(4))[0]
        h = struct.unpack(">I", f.read(4))[0]
    return w, h

W_PX, H_PX = png_size(IMG)
# Word uses EMUs (914400 per inch). Render full text width (~6 in landscape, ~6.3 in normal Word).
TARGET_IN = 6.3
PX_PER_IN = 96
RATIO = H_PX / W_PX
W_EMU = int(TARGET_IN * 914400)
H_EMU = int(TARGET_IN * RATIO * 914400)

DISCLAIMER_XML = """<w:p><w:pPr><w:pStyle w:val="Normal"/></w:pPr>\
<w:r><w:rPr><w:i/><w:color w:val="555555"/></w:rPr>\
<w:t xml:space="preserve">Hinweis: Da wir am Projekt weiterarbeiten, koennen sich einzelne Inhalte und technische Details bis zur Praesentation noch aendern.</w:t>\
</w:r></w:p>"""

# A new section that fits the structure: "Netzwerk-Architektur" with embedded image
NET_SECTION_XML = """<w:p><w:pPr><w:pStyle w:val="Heading2"/></w:pPr>\
<w:r><w:t xml:space="preserve">2.7 Netzwerk-Architektur</w:t></w:r></w:p>\
<w:p><w:pPr><w:pStyle w:val="Normal"/></w:pPr>\
<w:r><w:t xml:space="preserve">Das Spiel laeuft als LAN-Multiplayer auf jedem PC, der entweder hosten oder beitreten kann. Die folgende Uebersicht zeigt die aktuelle Netzwerk-Architektur: zwei parallele EnTT-Registries (server-authoritativ + client-side), ein 20-Hz-Fixed-Tick mit gestaffelten Snapshot-Raten und 19 Pakettypen fuer Spawning, Snapshots, Befehle und Gameplay-Events. Transport: enet (UDP).</w:t></w:r></w:p>\
<w:p><w:pPr><w:pStyle w:val="Normal"/><w:jc w:val="center"/></w:pPr>\
<w:r><w:drawing>\
<wp:inline distT="0" distB="0" distL="0" distR="0" xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing">\
<wp:extent cx="{W}" cy="{H}"/>\
<wp:effectExtent l="0" t="0" r="0" b="0"/>\
<wp:docPr id="100" name="Netzwerk-Architektur"/>\
<wp:cNvGraphicFramePr><a:graphicFrameLocks xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" noChangeAspect="1"/></wp:cNvGraphicFramePr>\
<a:graphic xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main">\
<a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture">\
<pic:pic xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture">\
<pic:nvPicPr><pic:cNvPr id="100" name="Netzwerk-Architektur"/><pic:cNvPicPr/></pic:nvPicPr>\
<pic:blipFill><a:blip r:embed="rIdNetImg" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>\
<pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="{W}" cy="{H}"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr>\
</pic:pic></a:graphicData></a:graphic>\
</wp:inline></w:drawing></w:r></w:p>\
<w:p><w:pPr><w:pStyle w:val="Normal"/><w:jc w:val="center"/></w:pPr>\
<w:r><w:rPr><w:i/><w:color w:val="666666"/><w:sz w:val="18"/></w:rPr>\
<w:t xml:space="preserve">Abb.: Aktuelle Netzwerk-Architektur (Stand kurz vor der Praesentation).</w:t></w:r></w:p>\
<w:p><w:pPr><w:pStyle w:val="Normal"/></w:pPr>\
<w:r><w:rPr><w:b/></w:rPr><w:t xml:space="preserve">Aenderungen gegenueber der urspruenglichen Skizze: </w:t></w:r>\
<w:r><w:t xml:space="preserve">Die Anzahl der Pakettypen ist von 5 auf 19 gewachsen (u.a. UNIT_SPAWNED, BUILDING_SPAWNED, TERRITORY_SNAPSHOT, FOG_SNAPSHOT, COMMANDER_ORDER, UPGRADE_COMPLETED). EntitySnapshot synct jetzt Position + Rotation + Scale + Velocity (vorher nur Position + Velocity). PlayerInput uebertraegt zusaetzlich Yaw/Pitch der Kamera. Snapshots laufen gestaffelt: Entities @ 20 Hz, Territory- und Fog-Snapshots @ 2 Hz. Lokale Prediction wurde in den per-Frame-LogicUpdate verschoben; Snapshots ueberschreiben die Rotation des eigenen Spielers nicht mehr. Der Szenen-/Layer-Aufbau ist jetzt in einen LayerStack mit GameLayer und DebugLayer eingebettet.</w:t></w:r></w:p>"""

NET_SECTION_XML = NET_SECTION_XML.replace("{W}", str(W_EMU)).replace("{H}", str(H_EMU))

# --- load document.xml + rels --------------------------------------------------
with zipfile.ZipFile(SRC, "r") as zin:
    files = {name: zin.read(name) for name in zin.namelist()}

doc = files["word/document.xml"].decode("utf-8")
rels = files["word/_rels/document.xml.rels"].decode("utf-8")
ct  = files["[Content_Types].xml"].decode("utf-8")

# Insert disclaimer right after the title paragraph (which contains "Projektportfolio")
# Strategy: insert before the first <w:p> that contains "TOC" or before "1. Spielkonzept".
# Simpler & robust: insert right after the first paragraph that contains "Projektportfolio".
m = re.search(r'(<w:p\b[^>]*>(?:(?!</w:p>).)*?Projektportfolio.*?</w:p>)', doc, flags=re.DOTALL)
assert m, "could not locate Projektportfolio paragraph"
end = m.end()
doc = doc[:end] + DISCLAIMER_XML + doc[end:]

# Insert net section right BEFORE the "3. Dokumentation" heading paragraph.
# Find the <w:p> containing "3. Dokumentation" or the existing "Doxygen-Dokumentation" heading.
m = re.search(r'(<w:p\b[^>]*>(?:(?!</w:p>).)*?Doxygen-Dokumentation.*?</w:p>)', doc, flags=re.DOTALL)
if not m:
    m = re.search(r'(<w:p\b[^>]*>(?:(?!</w:p>).)*?3\.\s*Dokumentation.*?</w:p>)', doc, flags=re.DOTALL)
assert m, "could not locate insertion point near section 3"

# Walk backward to find the Heading1 of "3." (so we insert BEFORE it).
# Doxygen-Dokumentation is Heading2 under "3." — go up one paragraph.
# Simpler: search for the Heading1 paragraph that introduces section 3.
m3 = re.search(r'(<w:p\b[^>]*>(?:(?!</w:p>).)*?3\.\s*Dokumentation.*?</w:p>)', doc, flags=re.DOTALL)
if m3:
    pos = m3.start()
else:
    pos = m.start()
doc = doc[:pos] + NET_SECTION_XML + doc[pos:]

# --- add image part + relationship --------------------------------------------
# Add the PNG into word/media/ and reference it as rIdNetImg
img_bytes = IMG.read_bytes()
files["word/media/netarch.png"] = img_bytes

# Append relationship (rIdNetImg → media/netarch.png)
if "rIdNetImg" not in rels:
    new_rel = '<Relationship Id="rIdNetImg" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="media/netarch.png"/>'
    rels = rels.replace("</Relationships>", new_rel + "</Relationships>")

# Ensure Content Types has a PNG default
if 'Extension="png"' not in ct:
    new_def = '<Default Extension="png" ContentType="image/png"/>'
    # Insert before </Types>
    ct = ct.replace("</Types>", new_def + "</Types>")

files["word/document.xml"]              = doc.encode("utf-8")
files["word/_rels/document.xml.rels"]   = rels.encode("utf-8")
files["[Content_Types].xml"]            = ct.encode("utf-8")

with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as zout:
    for name, data in files.items():
        zout.writestr(name, data)

print(f"OK  wrote {OUT}  ({OUT.stat().st_size//1024} KB)")
print(f"    image embed: {W_PX}x{H_PX} px  ->  {W_EMU/914400:.2f} x {H_EMU/914400:.2f} inch")
