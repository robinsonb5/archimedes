/*
 Simplistic FIFO for clock domain crossing in the Archimedes core.
 Written to replace an AltSyncRAM-based component
 to reduce block RAM usage.  Doesn't make any attempt to
 deal with underflow.
 Split data width, 32 bits in, 8 bits out.
*/

module cdcfifo #(parameter depth=2)
(
	input aclr,

	// Write side

	input wrclk,
	input wrreq,
	input [31:0] data,
	output [3:0] wrusedw,

	// Read side

	input rdclk,
	input rdreq,
	output [7:0] q
);

localparam words=2**depth;

// Storage
reg [31:0] storage [0:words-1] /* synthesis ramstyle="logic" */; 
reg rdreq_word;
reg [depth-1:0] rdptr;
reg [depth-1:0] wrptr;

reg [31:0] q_word_w;
always @(*)
	q_word_w <= storage[rdptr];


// Write side

always @(posedge wrclk,posedge aclr) begin
	if(aclr)
		wrptr<={depth{1'b0}};
	else begin
		if(wrreq) begin
			storage[wrptr]<=data;
			wrptr<=wrptr+1'b1;
		end
	end
end


// Read request pulse in the write clock domain
wire rdreq_w;
cdc_pulse cdc_req(.clk_d(rdclk),.d(rdreq_word),.clk_q(wrclk),.q(rdreq_w));

always @(posedge wrclk,posedge aclr) begin
	if(aclr)
		rdptr<={depth{1'b0}};
	else begin
		if(rdreq_w) begin
			rdptr<=rdptr+1'b1;
		end	
	end
end


// Fill counter

reg [3:0] wrcounter;
always @(posedge wrclk,posedge aclr) begin
	if(aclr) begin
		wrcounter <= 3'b0;
	end else begin
		case({wrreq,rdreq_w})
			2'b00 : ;
			2'b11 : ;
			2'b01 : wrcounter<=wrcounter-1'b1;
			2'b10 : wrcounter<=wrcounter+1'b1;		
		endcase
	end
end
assign wrusedw=wrcounter;


// Read side

reg [31:0] q_word;

reg [1:0] rdcounter;
always @(posedge rdclk,posedge aclr) begin
	if(aclr) begin
		rdcounter <= 2'b11;
	end else begin
		rdreq_word=1'b0;
		if(rdreq) begin
			if(rdcounter[1:0]==2'b11) begin
				rdreq_word=1'b1;
				q_word<=q_word_w;
			end
			rdcounter <= rdcounter+1'b1;
		end
	end
end

reg [7:0] q_out;
always @(*) begin
	case(rdcounter[1:0])
		2'b11 : q_out <= q_word[31:24];
		2'b10 : q_out <= q_word[23:16];
		2'b01 : q_out <= q_word[15:8];
		2'b00 : q_out <= q_word[7:0];
	endcase
end

assign q=q_out;

endmodule
