"""JLCPCB assembly files (BOM + CPL) from a KiCad project.

Only parts whose schematic symbol has an "LCSC" field are listed: those are the
ones JLC places. Everything else (through-hole, DNP) is soldered by hand and left out.

    python jlc_assembly.py <project .kicad_pro or folder> <output folder> [--kicad-cli PATH]

Writes <name>-bom-jlc.csv (Comment, Designator, Footprint, LCSC Part #) and
<name>-cpl-jlc.csv (Designator, Mid X, Mid Y, Layer, Rotation; mm).
Needs only the Python standard library and kicad-cli (KiCad 8+).
"""
import argparse
import csv
import io
import os
import re
import subprocess
import sys
import tempfile

DEFAULT_CLI = r"C:\Program Files\KiCad\10.0\bin\kicad-cli.exe"


def run(cli, *args):
    subprocess.run([cli, *args], check=True, stdout=subprocess.DEVNULL)


def lcsc_parts(netlist_text):
    """ref -> (value, footprint, lcsc) for every component with an LCSC field."""
    parts = {}
    for m in re.finditer(r'\(comp\s+\(ref "([^"]+)"\)\s+\(value "([^"]*)"\)\s+\(footprint "([^"]*)"\)([\s\S]*?)\n\t\t\)',
                         netlist_text):
        ref, value, footprint, body = m.groups()
        f = re.search(r'\(field\s+\(name "LCSC"\)\s+"([^"]+)"', body)
        if f and f.group(1).strip():
            parts[ref] = (value, footprint.split(":")[-1], f.group(1).strip())
    return parts


def ref_key(ref):
    m = re.match(r"([A-Za-z#]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("project")
    ap.add_argument("outdir")
    ap.add_argument("--kicad-cli", default=DEFAULT_CLI)
    a = ap.parse_args()

    proj = a.project
    if os.path.isdir(proj):
        pros = [f for f in os.listdir(proj) if f.endswith(".kicad_pro")]
        if len(pros) != 1:
            sys.exit(f"expected one .kicad_pro in {proj}, found {pros}")
        proj = os.path.join(proj, pros[0])
    base = proj[: -len(".kicad_pro")]
    name = os.path.basename(base)
    sch, pcb = base + ".kicad_sch", base + ".kicad_pcb"
    os.makedirs(a.outdir, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        net = os.path.join(tmp, "n.net")
        pos = os.path.join(tmp, "p.csv")
        run(a.kicad_cli, "sch", "export", "netlist", "-o", net, sch)
        run(a.kicad_cli, "pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "-o", pos, pcb)
        parts = lcsc_parts(open(net, encoding="utf8").read())
        rows = list(csv.DictReader(open(pos, encoding="utf8")))

    # BOM: one line per (value, footprint, LCSC)
    groups = {}
    for ref, (val, fp, lcsc) in parts.items():
        groups.setdefault((val, fp, lcsc), []).append(ref)
    bom_path = os.path.join(a.outdir, f"{name}-bom-jlc.csv")
    with open(bom_path, "w", newline="", encoding="utf8") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (val, fp, lcsc), refs in sorted(groups.items(), key=lambda g: ref_key(sorted(g[1], key=ref_key)[0])):
            w.writerow([val, ",".join(sorted(refs, key=ref_key)), fp, lcsc])

    # CPL: only the parts JLC places; KiCad's Y axis points down, the pos export already flips it
    cpl_path = os.path.join(a.outdir, f"{name}-cpl-jlc.csv")
    placed, missing = 0, set(parts)
    with open(cpl_path, "w", newline="", encoding="utf8") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for r in sorted(rows, key=lambda r: ref_key(r["Ref"])):
            if r["Ref"] not in parts:
                continue
            missing.discard(r["Ref"])
            layer = "Top" if r["Side"].lower() == "top" else "Bottom"
            rot = float(r["Rot"]) % 360
            w.writerow([r["Ref"], f'{float(r["PosX"]):.4f}mm', f'{float(r["PosY"]):.4f}mm', layer, f"{rot:g}"])
            placed += 1

    print(f"{name}: {len(groups)} BOM lines, {placed} placements -> {bom_path}, {cpl_path}")
    if missing:
        print("WARNING: LCSC parts not found on the board:", ", ".join(sorted(missing, key=ref_key)))
        sys.exit(1)


if __name__ == "__main__":
    main()
