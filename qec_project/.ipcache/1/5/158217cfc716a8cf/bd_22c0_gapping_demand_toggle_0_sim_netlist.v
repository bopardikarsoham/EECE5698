// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2023.1 (lin64) Build 3865809 Sun May  7 15:04:56 MDT 2023
// Date        : Thu Sep 24 15:44:44 2026
// Host        : pc159.test-fpgaproject.octfpga-pg0.cloudlab.umass.edu running 64-bit Ubuntu 22.04.2 LTS
// Command     : write_verilog -force -mode funcsim -rename_top decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix -prefix
//               decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix_ bd_22c0_gapping_demand_toggle_0_sim_netlist.v
// Design      : bd_22c0_gapping_demand_toggle_0
// Purpose     : This verilog netlist is a functional simulation representation of the design and should not be modified
//               or synthesized. This netlist cannot be used for SDF annotated simulation.
// Device      : xcu280-fsvh2892-2L-e
// --------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CHECK_LICENSE_TYPE = "bd_22c0_gapping_demand_toggle_0,c_counter_binary_v12_0_16,{}" *) (* downgradeipidentifiedwarnings = "yes" *) (* x_core_info = "c_counter_binary_v12_0_16,Vivado 2023.1" *) 
(* NotValidForBitStream *)
module decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix
   (CLK,
    CE,
    Q);
  (* x_interface_info = "xilinx.com:signal:clock:1.0 clk_intf CLK" *) (* x_interface_parameter = "XIL_INTERFACENAME clk_intf, ASSOCIATED_BUSIF q_intf:thresh0_intf:l_intf:load_intf:up_intf:sinit_intf:sset_intf, ASSOCIATED_RESET SCLR, ASSOCIATED_CLKEN CE, FREQ_HZ 50000000, FREQ_TOLERANCE_HZ 0, PHASE 0, CLK_DOMAIN cd_ctrl_00, INSERT_VIP 0" *) input CLK;
  (* x_interface_info = "xilinx.com:signal:clockenable:1.0 ce_intf CE" *) (* x_interface_parameter = "XIL_INTERFACENAME ce_intf, POLARITY ACTIVE_HIGH" *) input CE;
  (* x_interface_info = "xilinx.com:signal:data:1.0 q_intf DATA" *) (* x_interface_parameter = "XIL_INTERFACENAME q_intf, LAYERED_METADATA xilinx.com:interface:datatypes:1.0 {DATA {datatype {name {attribs {resolve_type immediate dependency {} format string minimum {} maximum {}} value data} bitwidth {attribs {resolve_type generated dependency bitwidth format long minimum {} maximum {}} value 1} bitoffset {attribs {resolve_type immediate dependency {} format long minimum {} maximum {}} value 0} integer {signed {attribs {resolve_type immediate dependency {} format bool minimum {} maximum {}} value false}}}} DATA_WIDTH 1}" *) output [0:0]Q;

  wire CE;
  wire CLK;
  wire [0:0]Q;
  wire NLW_U0_THRESH0_UNCONNECTED;

  (* C_AINIT_VAL = "0" *) 
  (* C_CE_OVERRIDES_SYNC = "0" *) 
  (* C_COUNT_BY = "1" *) 
  (* C_COUNT_MODE = "0" *) 
  (* C_COUNT_TO = "1" *) 
  (* C_FB_LATENCY = "0" *) 
  (* C_HAS_CE = "1" *) 
  (* C_HAS_LOAD = "0" *) 
  (* C_HAS_SCLR = "0" *) 
  (* C_HAS_SINIT = "0" *) 
  (* C_HAS_SSET = "0" *) 
  (* C_HAS_THRESH0 = "0" *) 
  (* C_IMPLEMENTATION = "0" *) 
  (* C_LATENCY = "1" *) 
  (* C_LOAD_LOW = "0" *) 
  (* C_RESTRICT_COUNT = "0" *) 
  (* C_SCLR_OVERRIDES_SSET = "1" *) 
  (* C_SINIT_VAL = "0" *) 
  (* C_THRESH0_VALUE = "1" *) 
  (* C_VERBOSITY = "0" *) 
  (* C_WIDTH = "1" *) 
  (* C_XDEVICEFAMILY = "virtexuplusHBM" *) 
  (* downgradeipidentifiedwarnings = "yes" *) 
  (* is_du_within_envelope = "true" *) 
  decalper_eb_ot_sdeen_pot_pi_dehcac_xnilix_c_counter_binary_v12_0_16 U0
       (.CE(CE),
        .CLK(CLK),
        .L(1'b0),
        .LOAD(1'b0),
        .Q(Q),
        .SCLR(1'b0),
        .SINIT(1'b0),
        .SSET(1'b0),
        .THRESH0(NLW_U0_THRESH0_UNCONNECTED),
        .UP(1'b1));
endmodule
`pragma protect begin_protected
`pragma protect version = 1
`pragma protect encrypt_agent = "XILINX"
`pragma protect encrypt_agent_info = "Xilinx Encryption Tool 2023.1"
`pragma protect key_keyowner="Synopsys", key_keyname="SNPS-VCS-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
rRTOhyxlrYBSSl4vXOJsCov/PYTcGQv0hZAEWOT0hXiNol44n1/uH9QDDV2d6jw2JngQv7TlJPZr
ks3uCeCjkKNEWnRoMqLLTYocNlzPK8/C69vks1NsEThxNhNC+ZsJsgirzk+kgzY9kn0FeBByVNqf
jvMBsh9uVgRGd4uC1wk=

`pragma protect key_keyowner="Aldec", key_keyname="ALDEC15_001", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
01nKGPawFhPZM6IfhQEjhBpb1VBWTCn/eVyNPM0YoKiJQV9nou366lnD6Z7IbogTZYpQ+S4S46GW
PUw6z6hJ+9NtiS3jFFu6T9LFXPfNcraug8eCrLWiosznuSBj9mp+1Bol3rqM7js96zbwYO0gqweI
QolQG37MZX2LrNEtgf5D1fnq4deWVQfLA/P2WGX5Hnk//FeuACvmWvGb24p41b9RIe2VpYd8xEQz
A6AtDqFJvar62K48Gzx6gqqLAeVwxFUAgFb2nNrwFHikIbJfIt3VXvF2yBr2Wv1aHj98N3zwMHG9
E+x52jYbkwxMgPOUm/EflFroqLTP230jaGZVkQ==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VELOCE-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=128)
`pragma protect key_block
OgWk/5sEH+VtvUozJoY5vqwlh1efblM9NuO5cPvnN4s9tI520b4yVIDKiBCaB9QhJ4geHmQtgFDa
TMoDECveDjTlUvpvwUA91n9f4rTXBK36T3uiuOpqPSFEhM3PJq+nKh/nPjg3bo1//FOQPmo1DI0d
iNpWtFZ4Yuobaoyj1GA=

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-VERIF-SIM-RSA-2", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
avNU0fSLkoHFIh/jgPtDSi4L86Y2rAHF1WLjnPGw39ZrkztShDCTHzGPRVaSu06kikZjHwGr6OMy
T67c/p0aK2uSfhKd5uYwusuDWVNMJELM/HyeaR4JFtZGvAtAJkUblcBmbFlkcI4Z49bkp3pjhRbd
oqJsP4yHfbFSuZxkHkOYue3u1K0oQxwgcUrrdjDWx6H2y0fN4RGST5A1qM7gDvK44xvDnmrq1QrB
WuYnDcXDUTN49PurLq8sGWTloRUZZjyG4v4sJZAISb2iEoxXVn2Oir7rUqLsON9rjCa9ReXnwO3F
swiKFDINK5ClAXgbtTMvsfVN+O0IGNRU0aF8Vw==

`pragma protect key_keyowner="Real Intent", key_keyname="RI-RSA-KEY-1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
UTM7xXi12etbqCmw/O1eGBPAsmWm1WEG4doh1296UvN3zKFsM0XZSx6Ziz0WajROY06JopnqYS0x
/Ia1Lr6DyPP38PE1ShybMJtKmjZ1zST1OjYMejK7hPTbabUMFmwYT/OfK3ahjtEeWEYYfnIRrRE3
GL+AdqvYhvkGdkLCn0zHlZLa1P6wOKJhkHxUXMOuWxCZnYWb0dYA6q8lwdwL3BUPjWijs0xNXuXh
6AOa5hCO2AUfXuy2pN4DWMLLIWq/eZ22oKicS27mPAUmy/l18cQOtBty67ZhdWjLAuHZYfZEFmdl
p18LDWn1yJaT4uTzqqo3ScoOQYuZvujZL0BH/g==

`pragma protect key_keyowner="Xilinx", key_keyname="xilinxt_2022_10", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
bkTXl+yGo2kTZsZVV2g5kqW/E/Je2ArJQXMgsh7Q9SZbPJkPn9tIdLpVifXduO9BY3NvCUT1tBDC
B56m0NNTikMELxSmvDh9WftYnAGYoN02iguLJTuH7NzI23upb8elIic/BDxq/3iMFm8xDrY19K8r
75jJQT6UylYBGBBx3EYHAhhkCNFWaIbOsDhjr8Pkl48dB7itI7RuNe91ySXIjPfhqaqGYDlQq6iL
LD1ywn/GzcK0iOn1NCWZYrhfNG9npiJxnWJaBhRu0SmuXk3A51Iq2la43BHz9A0mKQxuITXlO6k0
YvNhDl0Rb73kAzaok7jRBBxD/hZCpsaeIRbDKA==

`pragma protect key_keyowner="Metrics Technologies Inc.", key_keyname="DSim", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
U6/adMPoVlZkpPldLQcCHpV4MnnkIQ0koS1BgPRFbzVzgmAT5+90B+/aOuXDsHsTY04FiiPSIkLy
X/dO2qbqvjwOlU7hL5YLfc7M3R6VGFAtCeh26ScKD1rcCEXnk7okSxg+nBgOEdWfQxMWv/Igc1G7
ficf74VbUGXBK7sGFqUiU3Sju74MYsBIWQFL81s+d4noxooslx07SsduB1MZnNo98wPrfeHGRc5I
tOoy1ZWRRBurKqRaweusWsq2353jR1aRe0dis1d2TD9E5IRN04tRbRPxnvaqt5TbI6C0KKVGmJgA
dg5xKX8dUTm5P6EU7CJb0PIRstzyS8dbEWj6Mw==

`pragma protect key_keyowner="Atrenta", key_keyname="ATR-SG-RSA-1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=384)
`pragma protect key_block
l62S5HJJngOx7wKWqzRxn8l+Zg34fSA30rTACPQntRpmLTvPCtB/ZO7N2geVIGZC5xlFKur3ypkk
gu7fuj69QZIIf8cA+q5rFgQLKPTV5Jjb56A3m0e1ryIz1HNhekqxB2A3c3BoEM76CvsgwTCuJ54S
4itGKkmT3kSVqW+obJEyNczHp7v9brZlaNTGjLv/R++veL6PEriPMPaTCtM2GqlrRY0BEMamlh3G
G6HR53gprRQpNRt5ZXkgLBQgfAH6tcEUp2spoJfm5XQ0FJd/iuCRvaxcZRZ2SleGB7w4ohuSvU6D
FE5p5qm5ki1c8Q/lkBoNee27G3UYyYFvIVXUZpcrWdQc+9IXYqSZcB7lofhHMrNyGoJS1iwhFvZK
D3F5wNGYRvhmbp3scu/9bG1yieO8z89TMhn88AKkroCRONKExS2ADS+GyaI9cssXVbYPckioLbtI
83vBK/BOGY2haiXenyOeb5nBchvy+BuOhGB7yNbOBo5FjCpHK3NUmnTD

`pragma protect key_keyowner="Cadence Design Systems.", key_keyname="CDS_RSA_KEY_VER_1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
EB6FjmYfx5AnR6fqTJUDdNb8c6YI6fwnWVddbslCiiAEGWvpdNhCI94KZF1FewEBx/Y7HBBzOaPn
yIlFlswHYGGMlfTegCY7JSiCoX/xAYVWCe9+j+jE1HzhweCqfGP4z8wS5adANRari25fd3QTJfRH
VT9Xk/f2wud81Esu6X0vYYR/Wr9r9gyO/pxR2OgM2A+/LUA4EP4y2PwfdybHcbyhUc+MukeYeuLY
a9ZCUnEUm1GoQ8x1SOVTINVuU2py9zm9rU35K7RKCKr1bWiFQ6fN9YJc37GaxwMNRwOkltL1TXgH
uUy5c0CgX23BbSV2nXDmlCUYQbiQwIH8I0RV4Q==

`pragma protect key_keyowner="Mentor Graphics Corporation", key_keyname="MGC-PREC-RSA", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
U2r0rt9VpspHcRAf8LEzKnhTSWZPFC/8/uVDQHafy48NT9DOHov4eAZBkd4UKm4Q6j500EEcBcWy
lHFeaxbo69XMkV/7iPyR8Hjrg9yfOrjUtKkOpZe2fIa7cxg0lPXrnKBJhf3ug8LHwPgDCnPAoRoM
1HfGJ0lA3QtBvT4OVMRMUnp42BjnmEZ1I7t9gpyFvcmx9iA+vEK/fMAbA3s74EuJ4L7yASoYIsV+
TG9YFbrFXESmJV0joLmRthP//02acbbqN3uvgYi3TO9XtdMPMcqiZMIb62kPSxMj6nXEC9MFYSzY
zSll11URew02gx5ShNLnYrdXPCRpHuqRg75UbQ==

`pragma protect key_keyowner="Synplicity", key_keyname="SYNP15_1", key_method="rsa"
`pragma protect encoding = (enctype="BASE64", line_length=76, bytes=256)
`pragma protect key_block
isIrpAJM+F20aPEq4E0qIga/7p5BUqa8V9Fofs42Yw7nGABY5o7xTm/ZqBaM5HG/bovyQ5wG+B+z
7jjckKvBDQ+3LVsBqjLMnA4bBbLQIivzxLRPdiPIfs+xlmcsFGpJiNqXIG4IwHZipE2SF7oqzy67
NmNaJz7KG126lUHiS2Vfn8IZsn3H8IHK+iDhEXyAEAXDBUEheT1BqXtvHBx3bwO4CzFD8t9/ePjL
Et+rne4twanNrqvJrkGoorVJ96lzkDtFvNG93cpoJPVBGiR80QINmtZSI/08f5XIcMwUBVQsz2AN
U0ViJwI6Ysfvtlscsn9hhjhNSF2KlnXQ9T7ZeA==

`pragma protect data_method = "AES128-CBC"
`pragma protect encoding = (enctype = "BASE64", line_length = 76, bytes = 5168)
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
