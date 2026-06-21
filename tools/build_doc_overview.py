"""Render docs/mainpage.md to a single, self-contained HTML page.

Output: docs/generated/Bugmin-Dokumentation.html

The page is meant to sit next to the Doxygen `Detaillierte-Referenz/` folder
in the portfolio submission bundle. The teacher opens this one HTML file and
clicks through to detail pages only if she wants to.
"""
import re
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parent.parent
MD   = ROOT / "docs" / "mainpage.md"
OUT  = ROOT / "docs" / "generated" / "Bugmin-Dokumentation.html"

# --- transform inline references like `ClassName` (`path/file.h`) into links
# into the Doxygen reference folder. Doxygen mangles names: camelCase →
# _camel_case, so we approximate by lowercasing and inserting underscores
# before each capital. Good enough for the references we use in mainpage.md.
def doxy_anchor(name: str) -> str:
    """ClassName -> class_class_name.html  (best-effort, matches Doxygen)."""
    out = []
    for i, ch in enumerate(name):
        if ch.isupper() and i > 0 and not name[i-1].isupper():
            out.append("_")
        out.append(ch.lower())
    snake = "".join(out)
    return f"Detaillierte-Referenz/class_{snake}.html"

md_text = MD.read_text(encoding="utf-8")
body_html = markdown.markdown(md_text, extensions=["extra", "toc"])

# Replace the trailing GitHub link line note about "siehe Reference" — none yet.

CSS = r"""
:root{
  --bg:#fafbfc; --fg:#222; --muted:#666; --border:#dfe2e8;
  --accent:#3a78c2; --accent2:#3aa46a; --code-bg:#f3f5f9;
}
*{box-sizing:border-box}
body{
  margin:0;
  font-family:"Segoe UI",-apple-system,Arial,sans-serif;
  font-size:15px; line-height:1.6;
  color:var(--fg); background:var(--bg);
}
main{max-width:880px; margin:0 auto; padding:48px 32px 96px;}
h1{font-size:28px; border-bottom:2px solid var(--accent); padding-bottom:8px; margin-top:0;}
h2{font-size:21px; margin-top:36px; border-bottom:1px solid var(--border); padding-bottom:4px;}
h3{font-size:17px; margin-top:24px;}
p, li{color:var(--fg);}
blockquote{
  border-left:4px solid var(--accent);
  background:#eef3fa; margin:18px 0; padding:10px 16px;
  color:#234; border-radius:0 6px 6px 0;
}
code{
  background:var(--code-bg); padding:1px 5px; border-radius:3px;
  font-family:"Cascadia Mono","Consolas",monospace; font-size:0.92em;
}
pre{background:var(--code-bg); padding:12px 14px; border-radius:6px; overflow-x:auto;}
pre code{background:none; padding:0;}
table{border-collapse:collapse; margin:16px 0; width:100%;}
th, td{border:1px solid var(--border); padding:8px 12px; text-align:left; vertical-align:top;}
th{background:#eef3fa;}
a{color:var(--accent); text-decoration:none;}
a:hover{text-decoration:underline;}
hr{border:none; border-top:1px solid var(--border); margin:32px 0;}
.header{
  background:linear-gradient(180deg,#eaf1fb,#fafbfc); padding:32px 32px 16px;
  border-bottom:1px solid var(--border); text-align:center;
}
.header .sub{color:var(--muted); font-size:14px;}
"""

HEAD = f"""<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<title>Bugmin – Code-Dokumentation</title>
<style>{CSS}</style>
</head>
<body>
<div class="header">
  <div class="sub">Projektdokumentation · zur Abgabe Info-Projekt Q12</div>
</div>
<main>
{body_html}
<hr>
<p style="color:var(--muted); font-size:13px;">
  Hinweis: Wenn du tiefer einsteigen möchtest, liegt im Unterordner
  <code>Detaillierte-Referenz/</code> die vollständige automatisch generierte
  Doxygen-Dokumentation (öffnen über <code>index.html</code> in diesem Ordner).
</p>
</main>
</body>
</html>
"""

OUT.write_text(HEAD, encoding="utf-8")
size = OUT.stat().st_size
print(f"wrote {OUT}  ({size//1024} KB)")
