// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2023.1 (lin64) Build 3865809 Sun May  7 15:04:56 MDT 2023
// Date        : Thu Sep 24 15:39:03 2026
// Host        : pc159.test-fpgaproject.octfpga-pg0.cloudlab.umass.edu running 64-bit Ubuntu 22.04.2 LTS
// Command     : write_verilog -force -mode funcsim -rename_top decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix -prefix
//               decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix_ bd_58f6_lut_buffer_0_sim_netlist.v
// Design      : bd_58f6_lut_buffer_0
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xcu280-fsvh2892-2L-e
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CHECK_LICENSE_TYPE = "bd_58f6_lut_buffer_0,lut_buffer_v2_0_0_lut_buffer,{}" *) (* DowngradeIPIdentifiedWarnings = "yes" *) (* X_CORE_INFO = "lut_buffer_v2_0_0_lut_buffer,Vivado 2023.1" *) 
(* NotValidForBitStream *)
module decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix
   (tdi_i,
    tms_i,
    tck_i,
    drck_i,
    sel_i,
    shift_i,
    update_i,
    capture_i,
    runtest_i,
    reset_i,
    bscanid_en_i,
    tdo_o,
    tdi_o,
    tms_o,
    tck_o,
    drck_o,
    sel_o,
    shift_o,
    update_o,
    capture_o,
    runtest_o,
    reset_o,
    bscanid_en_o,
    tdo_i);
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan TDI" *) input tdi_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan TMS" *) input tms_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan TCK" *) input tck_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan DRCK" *) input drck_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan SEL" *) input sel_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan SHIFT" *) input shift_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan UPDATE" *) input update_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan CAPTURE" *) input capture_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan RUNTEST" *) input runtest_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan RESET" *) input reset_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan BSCANID_EN" *) input bscanid_en_i;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 s_bscan TDO" *) output tdo_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan TDI" *) output tdi_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan TMS" *) output tms_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan TCK" *) output tck_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan DRCK" *) output drck_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan SEL" *) output sel_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan SHIFT" *) output shift_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan UPDATE" *) output update_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan CAPTURE" *) output capture_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan RUNTEST" *) output runtest_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan RESET" *) output reset_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan BSCANID_EN" *) output bscanid_en_o;
  (* X_INTERFACE_INFO = "xilinx.com:interface:bscan:1.0 m_bscan TDO" *) input tdo_i;

  wire bscanid_en_i;
  wire bscanid_en_o;
  wire capture_i;
  wire capture_o;
  wire drck_i;
  wire drck_o;
  wire reset_i;
  wire reset_o;
  wire runtest_i;
  wire runtest_o;
  wire sel_i;
  wire sel_o;
  wire shift_i;
  wire shift_o;
  wire tck_i;
  wire tck_o;
  wire tdi_i;
  wire tdi_o;
  wire tdo_i;
  wire tdo_o;
  wire tms_i;
  wire tms_o;
  wire update_i;
  wire update_o;
  wire [31:0]NLW_inst_bscanid_o_UNCONNECTED;

  (* C_EN_BSCANID_VEC = "0" *) 
  (* DONT_TOUCH *) 
  (* KEEP_HIERARCHY = "soft" *) 
  (* is_du_within_envelope = "true" *) 
  decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix_lut_buffer_v2_0_0_lut_buffer inst
       (.bscanid_en_i(bscanid_en_i),
        .bscanid_en_o(bscanid_en_o),
        .bscanid_i({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .bscanid_o(NLW_inst_bscanid_o_UNCONNECTED[31:0]),
        .capture_i(capture_i),
        .capture_o(capture_o),
        .drck_i(drck_i),
        .drck_o(drck_o),
        .reset_i(reset_i),
        .reset_o(reset_o),
        .runtest_i(runtest_i),
        .runtest_o(runtest_o),
        .sel_i(sel_i),
        .sel_o(sel_o),
        .shift_i(shift_i),
        .shift_o(shift_o),
        .tck_i(tck_i),
        .tck_o(tck_o),
        .tdi_i(tdi_i),
        .tdi_o(tdi_o),
        .tdo_i(tdo_i),
        .tdo_o(tdo_o),
        .tms_i(tms_i),
        .tms_o(tms_o),
        .update_i(update_i),
        .update_o(update_o));
endmodule
`pragma protect begin_protected
`pragma protect version = 1
`pragma protect encrypt_agent = "XILINX"
`pragma protect encrypt_agent_info = "Xilinx Encryption Tool 2023.1"
`pragma protect key_keyowner="Synopsys", key_keyname="SNPS-VCS-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
PFWuFi9INDaYht5D9rexh/y7jPCksidWuo1Qj9P4/kg4g51mgSPxHO4BORjOYEBOl6igKk6B35Vw
KZn+XAeZUHtYsmme/Mlh7gXkxT5pWoBbSodqiBaU/fSabdC0eSPi3PGkEF8J6iaJ4gfBipmn7SIR
mjAJ0rLvEkIW/xq84Lk=

`pragma protect key_keyowner="Aldec", key_keyname="ALDEC15_001", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
yK0wZhqyxEbGOtcVASnw81GUBW94GwWDEYdacyCUXrEdTt4L277Iw4mXS8SXRMDHxcEHU3XkUemI
9Zw5aS2XBZUjD6AiRLmdmaaMy4PmL9Dqk60bvMVsGzZHOuA1WdeIe9DrfyJu5GfgmIvpobXASIfW
Spy9jf0acLwazzOqBZG2HZMLlYdfUDdca3dp5LbT99nIE4gZ+Lfth0ofw92g/3Tlme1tJ5BmFa6a
TDifQVq16OpP5zDlVI/z2YIQJvYZrjE+eKeyQSX6uBxUMZEBygH8jafWSSvPT9rAuecd3BmSWL7F
Lp4TTPBa83LRlG0O2MAe+7HxUNBv3F75IHzwkw==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VELOCE-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
LOs5LyF85q1EGknNJJwyStxjeLH/tRt6DtOKGxgNfzrksqJZQmdNh6ibryGbLm2vuVsgs6deHA07
xiCS1X7hKo06PfIkiDEBMAhhVG3OGXORbrnZ34GHPPvYh1ZCZAlv9LaVCmDC1LtSt0JrmCO+fNVF
AG0n4RUPeouYVMsXPQU=

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VERIF-SIM-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
OzN4Eh/yuqMyftOotmPdXDvvHpfPcbZaDgplkZX053Q6FgQ1rz2s+Ah62BJR8Wf9QyhwRCwYmZ9x
7EYfqN5EkBKMxueqGKb/JLQTPIwFzB6OjmJiQJ4bxJ7pNILTYxFngHGGhXdlA5tSTzMRUgBOkBA9
qKFJ51C0CvwLKAMTYim13QiLOjDIUJzEltpqNlZuMarSdx7hcyZl8Z+sbRjnBDnmZv6VNCMpwB/Y
0TLwXCnYyVHeosW/TgkuoB0i8hBalboPdqWffYPFAuxkb16PuB1PT1US1RJQoneomFSLpAq8TPwA
C3MJmHs/2BevtB7a4glpX5rTjRH8SH9J/6mcxQ==

`pragma protect key_keyowner="Real Intent", key_keyname="RI-RSA-KEY-1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
mmn3F07aMaQELmsoIAOLTXobjRPX6+jJGYxbHjAwiv+5GYbu4U1QHqM/n9MWKs5scXwRTklohbAA
sYC6LYGUnASoZ4cRBHxixylxt20YJ12cKi/TBunCWqC3d1aevmfIM3Lxvd2szBsA0T/7TB+3Nnqq
3yB3OVYDS8gXxBp1SL7Bbyay1yUzPEH0tWuf75xMp6gXEj+/D3mhiNWlqmR47ouullerBYE94msU
VtNmb8++jomCzXKIwNqiVjyt3v1FCh69YGeVxGmlVUm/prdbHGGLTDGxNYyn+CzLLISB/XYYwlX3
2jRBVvI5RU/Rji1AMzFUT+1WSuBT8DAY4qjB8w==

`pragma protect key_keyowner="Metrics Technologies Inc.", key_keyname="DSim", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
belPtlwUb6tkWZoO/daP4JnSM4Ix9rwO0UINU6lqKRLWU84wh6RRE5L5Vjp7Ep2NKjxmxr/fIx/N
nOeAqkGET10Mnh7wtBqbrKaOaCh+9VwSFg5ZktuhPIA3ieYV25JNzxXg28xOF96ruw5Ta/Q+/mon
VJO3bVWKtTG4dvy5mmOdOK5wKwaMN/UZcBhEg8AZaiG76DOv0DEFCyEkUOJb+Um8ZtHye6Xhwot3
lD84DJmgfiwLJCVmr8I0o4gSEKuwYlz03yx0ntqaleN40mb2K5mIsOUPB0gV9p6UDkllmgrVK2KK
a2zoG1baxmRZ7Gp7QUmaNDbn6o3uod+lZl1ozA==

`pragma protect key_keyowner="Xilinx", key_keyname="xilinxt_2022_10", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
hMbTrg3QVspzwQdvTAHkjT/KH2DRuL4CUyt/0Z5sjxyAaKTIMbIN/jZ+x3/TVS7NDePtu73D+mIG
+1fimH1EvpxpO9wE52LKEu3+y1igFN48hrK0QC4n/Z+tC6q/RgTvWlHV7kTYPcbZBJRi2XjtxByP
gbjAptSbmf4aSQn2aAoeYxtP1Y3SRmdsDMPmMlL5luJ5+sgvJDoydsm3Xz1XOr9lw5MhUN1GtPYd
S13wonWfhYt5FtCApi1VlHFtyA6NOyV4Dj5BS+7h5hLUoJkfkVbnwIQJzp1NAQ/AQA8Gl/Y2emTn
QLJFWilKDxi+FR31oVrwbrItFl8q/GwGJ6EGRQ==

`pragma protect key_keyowner="Atrenta", key_keyname="ATR-SG-RSA-1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=384)
`pragma protect key_block
OvQ8gaRgp5yYmvledOQGZsF3nMK1WKgp1m0ZXgW9QOFZZPK6omrLMcJ+aMLFCvGsc2GldXXcBtZM
hI33jy47IvykTcRD7Jr3IkOHudh3GNSgi5gqpVnFZuFnpYrW5hAuv6BW+ZgHZsKE/Q5lrUsGdAyw
OhR3UqqHX0VPBsI60QzwCFgZHboHi6bHiPWonZ8Ei7ULxB6XdSDeRqesb+LFyRysa6W6GT7XNRf4
1Xk2R7SLKLqwMA9SFHWBRiMumtr6hxj49qcS90XSKRPhmwFBVnGGqA4xEBhzgtaVM4U073AYQxMW
hfrebm/4LNV4OAQXLh5LGP76og9xB7KScZtxzWghei75OvTHhXoFQAqyH/saIlgAKwgLsVoJPkqS
9sZwnN0Mb2MvI9Des4YZ1/ESDb2qAFvo8ewiS3UxzeWyOKcEcS2ATkSXzz5wxf/kzUxeMI3kiC6M
3meU3wbsXLJu24QLstvkpPPSaIO9420R2GD7EIb56DvSP6rR4FsrqBXr

`pragma protect key_keyowner="Cadence Design Systems.", key_keyname="CDS_RSA_KEY_VER_1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
RPDrNu9XPTEchh9FQy18nOywOdqS/pwDBk2qsCrp5JG4kr2q8SlG7FMRGGXaPxwAmotmynZ0TZSW
7inea8lC1WTnsd9WblO6l+6l51GMUR5O1j20+bZdJeMidt/7zLoR5O0E1s6W0glfdqVX6qG4DtxY
Li723KHl8S3wBTQDQ3Hw6FoD8/XszNJOEysd55XJ0/FHh9BHFL7oxT7mYtunD1F06xqdW7S8Kf9f
qj72ulLja6JmwnN3+KIiumCpH7gjw/Re4H9lvnIns32Y3SBWDxaVL0Koo0/33ivaHV6uRH0je8uY
4nm/pmzNE4N/Hly2hyPPcli2JFwE0xMlFNWwFw==

`pragma protect data_method = "AES128-CBC"
`pragma protect encoding = (enctype = "BASE64", line_length = 76, bytes = 2960)
`pragma protect data_block
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
`pragma protect end_protected
`ifndef GLBL
`define GLBL
`timescale  1 ps / 1 ps

module glbl ();

    parameter ROC_WIDTH = 100000;
    parameter TOC_WIDTH = 0;
    parameter GRES_WIDTH = 10000;
    parameter GRES_START = 10000;

//--------   STARTUP Globals --------------
    wire GSR;
    wire GTS;
    wire GWE;
    wire PRLD;
    wire GRESTORE;
    tri1 p_up_tmp;
    tri (weak1, strong0) PLL_LOCKG = p_up_tmp;

    wire PROGB_GLBL;
    wire CCLKO_GLBL;
    wire FCSBO_GLBL;
    wire [3:0] DO_GLBL;
    wire [3:0] DI_GLBL;
   
    reg GSR_int;
    reg GTS_int;
    reg PRLD_int;
    reg GRESTORE_int;

//--------   JTAG Globals --------------
    wire JTAG_TDO_GLBL;
    wire JTAG_TCK_GLBL;
    wire JTAG_TDI_GLBL;
    wire JTAG_TMS_GLBL;
    wire JTAG_TRST_GLBL;

    reg JTAG_CAPTURE_GLBL;
    reg JTAG_RESET_GLBL;
    reg JTAG_SHIFT_GLBL;
    reg JTAG_UPDATE_GLBL;
    reg JTAG_RUNTEST_GLBL;

    reg JTAG_SEL1_GLBL = 0;
    reg JTAG_SEL2_GLBL = 0 ;
    reg JTAG_SEL3_GLBL = 0;
    reg JTAG_SEL4_GLBL = 0;

    reg JTAG_USER_TDO1_GLBL = 1'bz;
    reg JTAG_USER_TDO2_GLBL = 1'bz;
    reg JTAG_USER_TDO3_GLBL = 1'bz;
    reg JTAG_USER_TDO4_GLBL = 1'bz;

    assign (strong1, weak0) GSR = GSR_int;
    assign (strong1, weak0) GTS = GTS_int;
    assign (weak1, weak0) PRLD = PRLD_int;
    assign (strong1, weak0) GRESTORE = GRESTORE_int;

    initial begin
	GSR_int = 1'b1;
	PRLD_int = 1'b1;
	#(ROC_WIDTH)
	GSR_int = 1'b0;
	PRLD_int = 1'b0;
    end

    initial begin
	GTS_int = 1'b1;
	#(TOC_WIDTH)
	GTS_int = 1'b0;
    end

    initial begin 
	GRESTORE_int = 1'b0;
	#(GRES_START);
	GRESTORE_int = 1'b1;
	#(GRES_WIDTH);
	GRESTORE_int = 1'b0;
    end

endmodule
`endif
