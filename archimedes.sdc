
#**************************************************************
# Create Generated Clock
#**************************************************************

derive_pll_clocks

set sdram_clk "${topmodule}CLOCKS|altpll_component|auto_generated|pll1|clk[0]"
set mem_clk   "${topmodule}CLOCKS|altpll_component|auto_generated|pll1|clk[1]"
set sys_clk   "${topmodule}CLOCKS|altpll_component|auto_generated|pll1|clk[2]"
set vidc_clk  "${topmodule}CLOCKS_VIDC|altpll_component|auto_generated|pll1|clk[0]"
set vidc2x_clk "${topmodule}CLOCKS_VIDC|altpll_component|auto_generated|pll1|clk[1]"

create_generated_clock -name sdram_clk_ext -source [get_pins $sdram_clk] [get_ports $RAM_CLK]

#**************************************************************
# Set Clock Latency
#**************************************************************


#**************************************************************
# Set Clock Uncertainty
#**************************************************************

derive_clock_uncertainty;

#**************************************************************
# Set Input Delay
#**************************************************************

set_input_delay -clock [get_clocks sdram_clk_ext] -max 6.4 [get_ports ${RAM_IN}]
set_input_delay -clock [get_clocks sdram_clk_ext] -min 3.2 [get_ports ${RAM_IN}]


#**************************************************************
# Set Output Delay
#**************************************************************

set_output_delay -clock [get_clocks sdram_clk_ext] -max 1.5 [get_ports ${RAM_OUT}]
set_output_delay -clock [get_clocks sdram_clk_ext] -min -0.8 [get_ports ${RAM_OUT}]

set_output_delay -clock [get_clocks $vidc_clk] -max 0 [get_ports ${VGA_OUT}]
set_output_delay -clock [get_clocks $vidc_clk] -min -5 [get_ports ${VGA_OUT}]

#**************************************************************
# Set Clock Groups
#**************************************************************

set_clock_groups -asynchronous -group [get_clocks spiclk] -group [get_clocks ${topmodule}CLOCKS|*]
set_clock_groups -asynchronous -group [get_clocks spiclk] -group [get_clocks $vidc_clk]
# set_clock_groups -asynchronous -group [get_clocks spiclk] -group [get_clocks $vidc2x_clk]
set_clock_groups -asynchronous -group [get_clocks ${topmodule}CLOCKS|altpll_component|auto_generated|pll1|clk[*]] -group [get_clocks $vidc_clk]
set_clock_groups -asynchronous -group [get_clocks ${topmodule}CLOCKS|altpll_component|auto_generated|pll1|clk[*]] -group [get_clocks $vidc2x_clk]

#**************************************************************
# Set False Path
#**************************************************************

set_false_path -to [get_ports ${FALSE_OUT}]
set_false_path -from [get_ports ${FALSE_IN}]

#**************************************************************
# Set Multicycle Path
#**************************************************************
# SDRAM_CLK to internal sdram clock
set_multicycle_path -from [get_clocks sdram_clk_ext] -to [get_clocks $mem_clk] -setup 2

# system clock to internal sdram clock
set_multicycle_path -from [get_clocks $sys_clk] -to [get_clocks $mem_clk] -setup 2
set_multicycle_path -from [get_clocks $sys_clk] -to [get_clocks $mem_clk] -hold 1

set_false_path -to ${VGA_OUT}
set_false_path -to ${VGA_OUT}

#**************************************************************
# Set Maximum Delay
#**************************************************************



#**************************************************************
# Set Minimum Delay
#**************************************************************



#**************************************************************
# Set Input Transition
#**************************************************************
