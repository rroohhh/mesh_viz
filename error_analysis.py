#!/usr/bin/env python3

import mesh_viz
import matplotlib.pyplot as plt
import numpy as np
import sys
import matplotlib
matplotlib.use('TkAgg')

if __name__ == "__main__":
    print("loading {}", sys.argv[1])
    n = mesh_viz.load(sys.argv[1])
    n = [nn for nn in n if nn.x == 0 and nn.y == 1][0]
    d = n.data
    outstanding_var = d.subscopes["genblk_ports[0]"].subscopes["arq"].subscopes["wrapped"].subscopes["master_ins"].variables["outstanding"]
    print(outstanding_var.name)
    clk_var = d.variables["clk"]
    print(clk_var)
    times, values = n.read_values(outstanding_var, clk_var)
    print(times)
    print(values)
    plt.plot(times, values)
    plt.axhline(64, color="black")
    plt.show()



        # mo_var = var(f'genblk_ports[{d.value}].arq.wrapped.master_ins.outstanding')
        # mo_cap = mo_var.attrs["capacity"]
