module key_input
(
    input reset,
    input [7:0] keys_active,
    input [23:0] bus_address_in,
    output logic [7:0] bus_data_out
);

wire [7:0] reg_keys = reset ? 8'hFF: ~keys_active;

// @note: if the key irqs are not firing properly due to input being to short,
// do the following to latch the input.
//reg [8:0] key_irqs;
//reg [8:0] key_latches;
//always @ (posedge clk_sys)
//begin
//    for(int i = 0; i < 9; ++i)
//    begin
//        if(keys_active[i])
//            key_latches[i] <= 1;
//    end
//
//    if(clk_ce)
//    begin
//        for(int i = 0; i < 9; ++i)
//        begin
//            if(key_latches[i])
//            begin
//                key_irqs[i]    <= 1;
//                key_latches[i] <= 0;
//            end
//            else
//            begin
//                key_irqs[i] <= 0;
//            end
//        end
//    end
//end


always_comb
begin
    bus_data_out = 0;
    if(bus_address_in == 24'h2052)
        bus_data_out = reg_keys;
end

endmodule
