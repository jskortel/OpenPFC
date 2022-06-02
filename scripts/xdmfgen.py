#!/usr/bin/env python

"""
Generate xmf file for bunch of binary files, so that simulation results can be
opened easily using Paraview.
"""

import argparse
import glob
import os
import re
import json


header = """<?xml version="1.0"?>
<!DOCTYPE Xdmf SYSTEM "Xdmf.dtd" []>
<Xdmf xmlns:xi="http://www.w3.org/2001/XInclude" Version="2.0">
    <Domain>
        <Topology name="topo" TopologyType="3DCoRectMesh" Dimensions="{Lz} {Ly} {Lx}"></Topology>
        <Geometry name="geo" Type="ORIGIN_DXDYDZ">
            <DataItem Format="XML" Dimensions="3">{z0} {y0} {x0}</DataItem>
            <DataItem Format="XML" Dimensions="3">{dz} {dy} {dx}</DataItem>
        </Geometry>

        <Grid Name="TimeSeries" GridType="Collection" CollectionType="Temporal">
            <Time TimeType="HyperSlab">
                <DataItem Format="XML" NumberType="Float" Dimensions="{ntimesteps}">{timesteps}</DataItem>
            </Time>
"""

content = """            <Grid Name="{name}" GridType="Uniform"><Topology Reference="/Xdmf/Domain/Topology[1]" /><Geometry Reference="/Xdmf/Domain/Geometry[1]" /><Attribute Name="u" Center="Node"><DataItem Format="Binary" DataType="Float" Precision="8" Endian="Little" Dimensions="{Lz} {Ly} {Lx}">{f}</DataItem></Attribute></Grid>"""

footer = """
        </Grid>
    </Domain>
</Xdmf>"""


def extract_number(s):
    numbers = re.findall(r"\d+", s)
    assert len(numbers) == 1
    return int(numbers[0])


def main(args):
    if args["json"] is not None and os.path.exists(args["json"]):
        args.update(json.load(open(args["json"])))
    if args["results_dir"] != "" and not os.path.isdir(args["results_dir"]):
        print("Directory %s does not exist!" % args["results_dir"])
        return
    files = glob.glob(os.path.join(args["results_dir"], "*.bin"))
    if len(files) == 0:
        print("No data files found!")
        return
    files = sorted(files, key=lambda f: extract_number(os.path.basename(f)))
    expected_size = 8 * args["Lx"] * args["Ly"] * args["Lz"]
    if os.path.getsize(files[0]) != expected_size:
        print("File size mismatch, check lx, ly, lz!")
        return
    args["ntimesteps"] = len(files)
    args["timesteps"] = " ".join("%0.3f" % (i*args["saveat"]) for i in range(len(files)))
    print(header.format(**args))
    for f in files:
        print(content.format(**args, name=os.path.splitext(os.path.basename(f))[0], f=os.path.relpath(f)))
    print(footer.format(**args))


def cli():
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", default="input.json", help="read settings from json file (default input.json)")
    parser.add_argument("--results_dir", default="", help="data directory (default .)")
    parser.add_argument("--Lx", type=int)
    parser.add_argument("--Ly", type=int)
    parser.add_argument("--Lz", type=int)
    parser.add_argument("--x0", type=float)
    parser.add_argument("--y0", type=float)
    parser.add_argument("--z0", type=float)
    parser.add_argument("--dx", type=float)
    parser.add_argument("--dy", type=float)
    parser.add_argument("--dz", type=float)
    parser.add_argument("--dt", type=float)
    return vars(parser.parse_args())


if __name__ == "__main__":
    main(cli())
