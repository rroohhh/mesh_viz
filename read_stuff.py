#!/usr/bin/env python3

import imgui

def dump_var(n, name):
    value = n.get_current_var_value(n.data.variables[name])
    l = len(value)
    value = int(value, 2)
    imgui.text(name + " " + '{:#0{}x}'.format(value, 2 + l // 4))

def process(n):
    imgui.text(f"[{n.x}, {n.y}]")
    if (imgui.button("in")):
        n.add_var_to_viewer(n.data.variables["in_flit"])
        n.add_var_to_viewer(n.data.variables["in_valid"])
        n.add_var_to_viewer(n.data.variables["in_ready"])
        n.add_var_to_viewer(n.data.variables["clk"])

    # dump_var(n, "in_flit")
    # dump_var(n, "in_valid")
    # dump_var(n, "in_ready")
    # imgui.text("in_flit " + n.get_current_var_value(n.data.variables["in_flit"]));
    # imgui.text("flit_in valid " + n.get_current_var_value(n.data.variables["in_valid"]));
    # imgui.text("flit_in ready " + n.get_current_var_value(n.data.variables["in_ready"]));
    # for v, e in n.data.variables.items():
    #     imgui.text(e.name)
    # print(something.data.variables["in_flit"].name)
    # imgui.text("test")
    # print(imgui.button(f"python hello"))
    # if (imgui.button(f"hello")): #
        # data.append("1")
    # for entry in data:
    #     imgui.text("lol\nirlenas")

# lns{}
# process
# lambda a: print(a)
#print(__file__)
