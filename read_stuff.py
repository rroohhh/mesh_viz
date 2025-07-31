#!/usr/bin/env python3
import enum
import re
import numpy as np
from math import pi as π, sin, cos

def bb_contains(bb_a, bb_b, point):
    min_x = min(bb_a.x, bb_b.x)
    min_y = min(bb_a.y, bb_b.y)

    max_x = max(bb_a.x, bb_b.x)
    max_y = max(bb_a.y, bb_b.y)

    print(bb_a, bb_b, point)

    return point.x >= min_x and point.x <= max_x and point.y >= min_y and point.y <= max_y

N_VC = 2

import imgui
    # l = len(value)
    # value = int(value, 2)
    # imgui.text(name + " " + '{:#0{}x}'.format(value, 2 + l // 4))

class Dir(enum.Enum):
    NORTH = 0
    SOUTH = 1
    EAST = 2
    WEST = 3

    def t(self, v, mid):
        if self == Dir.NORTH:
            θ = 0
        elif self == Dir.SOUTH:
            θ = 180
        elif self == Dir.EAST:
            θ = 270
        elif self == Dir.WEST:
            θ = 90

        θ = θ / 180 * π
        mid = imgui.ImVec2(mid)
        d = imgui.ImVec2(v) - mid
        rx = d.x * cos(θ) - d.y * sin(θ)
        ry = d.x * sin(θ) + d.y * cos(θ)
        return mid + (rx, ry)

def vmin(a, b):
    if (a.x < b.x):
        return a
    else:
        return b

def vmax(a, b):
    if (a.x > b.x):
        return a
    else:
        return b

def process(n):
    text_color = n.text_color()
    scale = n.scale()
    def var(name):
        *scope, varname = name.split(".")
        s = n.data
        for sname in scope:
            s = s.subscopes[sname]
        return s.variables[varname]

    def val(v):
        if isinstance(v, str):
            v = var(v)
        return int(n.get_current_var_value(v), 2)

    clk_var = var("clk")
    # out_valid_var = var("out_valid")
    # out_ready_var = var("out_ready")

    def dump(d):
        def skip(name):
            if name.endswith("_payload"):
                return True
            if "$" in name:
                return True
            if "$" in name and name.split("$")[-2].endswith("_payload"):
                return True
            if name.startswith("cfg"):
                return True
        # print("hello", d.name)
        for name, subscope in d.subscopes.items():
            name = re.sub(r"(?<=genblk_ports\[)(\d)(?=\])", lambda m: Dir(int(m.group(1))).name, name)
            if skip(name):
                continue
            open = imgui.tree_node("t:" + name)
            if imgui.begin_popup_context_item():
                if imgui.selectable("add to viewer", False)[0]:
                    for v in subscope.variables.values():
                        n.add_var_to_viewer(v)
                if imgui.selectable("show histogram", False)[0]:
                    for v in subscope.variables.values():
                        n.add_hist(v, clk_var, [], [], True)
                if imgui.selectable("show histogram (out stream)", False)[0]:
                    for v in subscope.variables.values():
                        n.add_hist(v, clk_var, [out_valid_var, out_ready_var], [], True)
                imgui.end_popup()
            if open:
                dump(subscope)
                imgui.tree_pop()
        for name, v in d.variables.items():
            if skip(name):
                continue

            if "signal_flow_sample" in v.attrs or "signal_flow_sample_strobe" in v.attrs:
                continue
            if imgui.selectable(name, 0)[0]:
                n.add_var_to_viewer(v)
            if imgui.begin_popup_context_item(None):
                if imgui.selectable("add to viewer", False)[0]:
                    n.add_var_to_viewer(v)
                if imgui.selectable("show histogram", False)[0]:
                    n.add_hist(v, clk_var, [], [], True)
                if imgui.selectable("show histogram (out stream)", False)[0]:
                    n.add_hist(v, clk_var, [out_valid_var, out_ready_var], [], True)
                imgui.end_popup()


    draw = imgui.get_window_draw_list()

    def text_center_in(text, min, max):
        middle = (min + max) / 2
        sz = imgui.calc_text_size(text)
        avail_sz = max - min
        if abs(avail_sz.x) > sz.x and abs(avail_sz.y) > sz.y:
            draw.add_text(mid + middle - imgui.calc_text_size(text) / 2, text_color, text)



    sz = imgui.get_content_region_avail()
    min_pos = imgui.get_cursor_screen_pos()

    imgui.text(f"[{n.x}, {n.y}]")
    imgui.button("+")
    if imgui.begin_popup_context_item(None):
        dump(n.data)
        # if imgui.selectable("add to viewer", False)[0]:
        #     n.add_var_to_viewer(mo_var)
        # if imgui.selectable("show histogram", False)[0]:
        #     n.add_hist(mo_var, clk_var)
        imgui.end_popup()
    imgui.same_line()
    if imgui.button("++"):
        to_send = var("packets_to_send")
        sent = var("packets_sent")

        async def lol(an):
            ts_times, ts = await an.read_values(to_send, clk_var)
            s_times, s = await an.read_values(sent, clk_var)
            n.add_hist("outstanding packets", [to_send, sent], ts_times, ts - s)
            print(ts_times, ts, s_times, s)

        if imgui.is_key_down(imgui.Key.mod_ctrl):
            n.add_var_to_viewer(to_send)
            n.add_var_to_viewer(sent)
        else:
            n.enqueue_task(lol)


    mid = min_pos + sz / 2
     # / (8.0, 4.0)
    # print(sz)
    sz = sz / 2
    sz_r = sz / (4.0, 2.0)


    # formatting hack
    v = var(f"local_in__0__payload")

    for d in Dir:
        mo_var = var(f'{d.name.lower()}.arq_sender.outstanding')
        mo_cap = mo_var.attrs["capacity"]
        mo = val(mo_var)

        master_min = d.t((-sz_r.x, sz.y - sz_r.y), (0.0, 0.0))
        master_max = d.t((0.0, sz.y), (0.0, 0.0))
        master_max_fill = d.t((0.0, sz.y - sz_r.y * (1.0 - mo / mo_cap)), (0.0, 0.0))

        draw.add_rect(mid + master_min, mid + master_max, 0xff7f007f, thickness=2*scale)
        draw.add_rect_filled(mid + master_min, mid + master_max_fill, 0xff7f007f)
        imgui.set_cursor_screen_pos(mid + (min(master_min.x, master_max.x), min(master_min.y, master_max.y)))
        bsz = master_max - master_min
        bsz = (abs(bsz.x), abs(bsz.y))
        imgui.invisible_button(str(d) + "_tx", bsz)
        if imgui.begin_popup_context_item(None):
            if imgui.selectable("add to viewer", False)[0]:
                n.add_var_to_viewer(mo_var)
            if imgui.selectable("show histogram", False)[0]:
                print(n.read_values(mo_var, clk_var))
                n.add_hist(mo_var, clk_var, [], [], True)
            imgui.end_popup()
        text_center_in(str(mo), master_min, master_max)


        vc_s = sz_r.x / N_VC

        for vc in range(N_VC):
            to_var = var(f'{d.name.lower()}.input_mq_fifo.outstanding_{vc}')
            to_cap = to_var.attrs["capacity"]
            to = val(to_var)

            target_min = d.t((vc * vc_s, sz.y - sz_r.y), (0.0, 0.0))
            target_max = d.t(((vc + 1) * vc_s, sz.y), (0.0, 0.0))
            target_max_fill = d.t(((vc + 1) * vc_s, sz.y - sz_r.y * (1.0 - to / to_cap)), (0.0, 0.0))

            draw.add_rect(mid + target_min, mid + target_max, 0xffff0000, thickness=2*scale)
            draw.add_rect_filled(mid + target_min, mid + target_max_fill, 0xffff0000)

            imgui.set_cursor_screen_pos(mid + (min(target_min.x, target_max.x), min(target_min.y, target_max.y)))
            bsz = target_max - target_min
            bsz = (abs(bsz.x), abs(bsz.y))

            imgui.invisible_button(str(d) + "_rx" + str(vc), bsz)
            if imgui.begin_popup_context_item(None):
                if imgui.selectable("add to viewer", False)[0]:
                    n.add_var_to_viewer(to_var)
                if imgui.selectable("show histogram", False)[0]:
                    n.add_hist(to_var, clk_var, [], [], True)
                imgui.end_popup()

            text_center_in(str(to), target_min, target_max)


        if (d == Dir.NORTH and n.y > 0) or \
           (d == Dir.SOUTH and (n.y + 1) < n.system_config.width) or \
           (d == Dir.WEST and n.x > 0) or \
           (d == Dir.EAST and (n.x + 1) < n.system_config.height):
            # if (n.x + 1) < n.system_config.width:
            handle = mid + master_max + d.t((-sz_r.x / 2, 0), (0.0, 0.0))
            delta = d.t((0.0, sz.x / 1), (0.0, 0.0))
            tip = handle + delta
            tip_s = sz.x / 5
            tip_delta = d.t((-tip_s, -tip_s), (0.0, 0.0))
            draw.add_line(handle, handle + delta, text_color, thickness=2*scale)
            draw.add_line(tip, tip + tip_delta, text_color, thickness=2*scale)
            tip_delta = d.t((tip_s, -tip_s), (0.0, 0.0))
            draw.add_line(tip, tip + tip_delta, text_color, thickness=2*scale)


            # TODO(robin): use flit parsing here by including memory_mapped_router.py
            v = var(f"link_{d.name.lower()[0]}_o__payload")

            def pretty_up_label(label):
                label = re.sub(r"{payload=[^}]*} ,", ",", label)
                label = re.sub(r"payload=[^ ]* ,", "", label)
                return label

            font_factor = 1.0
            def packet_label(pos, text, var_value, t_size):
                if pos is not None:
                    io = imgui.get_io()
                    popup_id = "popup_id"
                    if imgui.is_mouse_released(1):
                        if bb_contains(pos, pos + t_size, io.mouse_pos):
                            imgui.open_popup(popup_id)
                    if imgui.begin_popup(popup_id):
                        if imgui.selectable("show trace", False)[0]:
                            # TODO(robin): very magic constant
                            shift = 10
                            if "start" in pretty:
                                shift = 24
                            # print(var_value)
                            # print(vv)
                            n.add_trace((var_value >> shift) & 0xffff_ffff)
                        imgui.end_popup()
                    draw.add_text(None, imgui.get_font_size() / font_factor, pos, text_color, text)

            vv = n.get_current_var_value(v)
            vv_num = int(vv, 2)
            if vv_num != 0:
                pretty = pretty_up_label(v.format(vv))

                t_sz = imgui.calc_text_size(pretty) / font_factor
                if t_sz.x < 8 * sz.x:
                    pos = None
                    if d == Dir.NORTH:
                        pos = handle - t_sz * (1.0, 0.0)
                    if d == Dir.SOUTH:
                        pos = handle - t_sz * (0.0, 1.0)
                    packet_label(pos, pretty, vv_num, t_sz)

            v = var(f"link_{d.name.lower()[0]}_i__p")
            vv = n.get_current_var_value(v)
            vv_num = int(vv, 2)
            if vv_num != 0:
                pretty = pretty_up_label(v.format(vv))

                t_sz = imgui.calc_text_size(pretty) / font_factor
                if t_sz.x < 8 * sz.x:
                    pos = None
                    if d == Dir.NORTH:
                        pos = handle - t_sz * (0.0, 0.0) + sz_r * (1.0, 0.0)
                        # draw.add_text(handle - t_sz * (0.0, 0.0) + sz_r * (1.0, 0.0), text_color, pretty)
                    if d == Dir.SOUTH:
                        pos = handle - t_sz * (1.0, 1.0) - sz_r * (1.5, 0.0)
                        # draw.add_text(handle - t_sz * (1.0, 1.0) - sz_r * (1.0, 0.0), text_color, pretty)
                    packet_label(pos, pretty, vv_num, t_sz)

            data_sent_var = var(f"{d.name.lower()}.data_sent")
            data_sent = n.get_current_var_value(data_sent_var)
            event_sent_var = var(f"{d.name.lower()}.event_sent")
            event_sent = n.get_current_var_value(event_sent_var)
            mux_count = len(data_sent)

            size = (sz.x - sz_r.x - mux_count) / mux_count
            h = sz_r.y / 8
            for i in range(mux_count):
                target_min = d.t((-sz_r.x - i * size - 4.0, sz.y - h), (0.0, 0.0))


                target_max = d.t((-sz_r.x- (i + 1) * size, sz.y - 3.0), (0.0, 0.0))
                col = 0xff_88_88_88
                if event_sent[i] == '1':
                    col = 0xff00ff00
                if data_sent[i] == '1':
                    col = 0xff0000ff
                draw.add_rect_filled(mid + target_min, mid + target_max, col)
            # print(data_sent, event_sent)


def bit_count(arr):
     # Make the values type-agnostic (as long as it's integers)
     t = arr.dtype.type
     mask = t(-1)
     s55 = t(0x5555555555555555 & mask)  # Add more digits for 128bit support
     s33 = t(0x3333333333333333 & mask)
     s0F = t(0x0F0F0F0F0F0F0F0F & mask)
     s01 = t(0x0101010101010101 & mask)

     arr = arr - ((arr >> 1) & s55)
     arr = (arr & s33) + ((arr >> 2) & s33)
     arr = (arr + (arr >> 4)) & s0F
     return (arr * s01) >> (8 * (arr.itemsize - 1))

def __main__(nodes):
    import numpy as np
    MUX_COUNT = 4 # TODO(robin): pass this through to sim

    n0 = [n for n in nodes if n.x == 0 and n.y == 0][0]
    out_valid_var = n0.data.variables["out_valid"]
    out_ready_var = n0.data.variables["out_ready"]
    clk_var = n0.data.variables["clk"]

    received = n0.read_values(n0.data.variables["flits_received"], clk_var, [out_valid_var, out_ready_var], [], True)[1][-1]

    max_latency = np.max(n0.read_values(n0.data.variables["flit_latency"], clk_var, [out_valid_var, out_ready_var], [], True)[1])

    sent = 0
    max_outstanding = 0
    for n in nodes:
        if n.x != 0 or n.y != 0:
            in_valid_var = n.data.variables["in_valid"]
            in_ready_var = n.data.variables["in_ready"]
            clk_var = n.data.variables["clk"]
            sent += n.read_values(n.data.variables["flits_sent"], clk_var, [in_valid_var, in_ready_var], [], True)[1][-1]
            p_to_send = n.read_values(n.data.variables["packets_to_send"], clk_var, [], [], True)[1]
            p_sent = n.read_values(n.data.variables["packets_sent"], clk_var, [], [], True)[1]
            max_outstanding = max(np.max(p_to_send - p_sent), max_outstanding)

    sc = nodes[0].system_config
    print(sc.error_params.direction)
    print(sc.height, sc.width, sc.link_delay, sc.node_params.packet_len, sc.node_params.p, sc.event_params.e, received, sent, max_latency, max_outstanding)
    for node in sorted(nodes, key = lambda n: n.y):
    # for node in nodes:
        clk = node.data.variables["clk"]
        for d in Dir:
            if (name := d.name.lower()) in node.data.subscopes:
                link = node.data.subscopes[name]
                link.variables["event_sent"]
                ev_time, ev_data = node.read_values(link.variables["event_sent"], clk, [], [], True)
                d_time, d_data = node.read_values(link.variables["data_sent"], clk, [], [], True)
                # print(ev_data)
                # print(ev_time)
                # print(d_data)
                # print(d_time)
                ev_count = bit_count(ev_data.astype(int))
                d_count = bit_count(d_data.astype(int))
                left_over = np.full_like(ev_count, MUX_COUNT) - ev_count
                print(node.x, node.y, d.name.lower(), np.sum(d_count), np.sum(left_over), np.sum(ev_count))
    # print(nodes)
    # print(n.get_current_var_value(var("south.data_sent")))
    # print(n.read_values(to_var, clk_var))
    # imgui.set_cursor_pos((0.0, 0.0))
    # if imgui.button("a"):
    #     n.add_var_to_viewer(var("clk"))
    # imgui.text(str(n.role.is_fpga))
    # imgui.text(str(n.system_config.rng_seed))


        # draw.add_text(0xffffffff, str(to))
        # break
        # imgui.text(f"{d.name}: {val()}")


    # def dump_var(n, var):
    #     value = n.get_current_var_value(var)
    #     imgui.text(var.name + " " + var.format(value))

    # imgui.text(f"[{n.x}, {n.y}]")
    # vars = []
    # for dir in range(4):
    #     s = n.data.subscopes[f"genblk1[{dir}]"].subscopes["arq"].subscopes["wrapped"]
    #     o1 = s.subscopes["master_ins"].variables["outstanding"]
    #     is_resend = s.subscopes["master_ins"].variables["is_resend"]
    #     o2 = s.subscopes["target_ins"].variables["outstanding"]
    #     vars += ([o1, o2, is_resend])
    #     o1 = n.get_current_var_value(o1)
    #     o2 = n.get_current_var_value(o2)
    #     imgui.text(f"dir: {dir}, o1: {o1}, o2: {o2}")
    # dump_var(n, var)
    # if (imgui.button("in")):
    #     var = n.data.subscopes["router_i"].subscopes["memory_mapped_router_internal"].variables["local_in__payload"]
    #     n.add_var_to_viewer(var)
        # for v in vars:
        #     n.add_var_to_viewer(v)
    #     n.add_var_to_viewer(n.data.variables["in_flit"])
    #     n.add_var_to_viewer(n.data.variables["in_valid"])
    #     n.add_var_to_viewer(n.data.variables["in_ready"])
        # n.add_var_to_viewer(n.data.variables["tx_accept_north"])
        # n.add_var_to_viewer(n.data.variables["tx_accept_south"])
        # n.add_var_to_viewer(n.data.variables["tx_accept_east"])
        # n.add_var_to_viewer(n.data.variables["tx_accept_west"])
        # for _ in range(100):
        #     n.add_var_to_viewer(n.data.variables["clk"])
        # print("hello")
        # n.add_var_to_viewer(var)

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
