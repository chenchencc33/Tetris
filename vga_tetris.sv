/*
 * Avalon memory-mapped peripheral that generates VGA
 * Columbia University
 */

module vga_tetris(
    input logic clk,
    input logic reset,
    input logic [15:0] writedata,
    input logic write,
    input chipselect,
    input logic [2:0] address,

    output logic [7:0] VGA_R, VGA_G, VGA_B,
    output logic VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n,
    output logic VGA_SYNC_n,

    // The music related IO.
    input avalon_streaming_source_l_ready, avalon_streaming_source_r_ready,
    output logic[15:0] avalon_left, avalon_right,
    output logic avalon_streaming_source_l_valid, avalon_streaming_source_r_valid
    );

    logic [10:0] hcount;
    logic [9:0] vcount;

    vga_counters counters(.clk50(clk), .*);

    // The pixels equal to 16 * 16 here.
    localparam SPR_PIXELS = 256;
    localparam COLR_BITS = 1;
    localparam SPR_ADDRW  = $clog2(SPR_PIXELS);
    logic [SPR_ADDRW - 1:0] rom_addr;
    logic [1:0] block_rom_data;
    logic [2:0] digit_rom_data[0:9];

    // The pixels equal to 128 * 512 here.
    localparam BG_PIXELS = 65536;
    localparam BG_ADDRW  = $clog2(BG_PIXELS);
    localparam BG_IDX_BITS = 8;
    logic [BG_ADDRW - 1:0] bg_addr;
    logic [BG_IDX_BITS - 1:0] bg_idx_rom_data[0:4];
    localparam BG_COLOR_BITS = 24;
    logic [6 - 1:0] bg_idx;
    logic [BG_COLOR_BITS - 1:0] bg_rom_data;

    // bgm
    logic [15:0] bgm_rom_data;
    logic [16:0] bgm_rom_addr;

    genvar i;
    generate
        // Background segements.
        for (i = 0; i < 5; i++) begin: generate_bg
            rom_async #(
                .WIDTH(BG_IDX_BITS),
                .DEPTH(BG_PIXELS),
                .INIT_F({"./image/bg_", "0" + i, ".mem"})
            ) bg0_rom (
                .addr(bg_addr),
                .data(bg_idx_rom_data[i]));
        end

        // The digits for scores
        for (i = 0; i < 10; i++) begin: generate_digits
            rom_async #(
                .WIDTH(3),
                .DEPTH(SPR_PIXELS),
                .INIT_F({"./image/", "0" + i, ".mem"})
            ) zero_rom (
                .addr(rom_addr),
                .data(digit_rom_data[i]));
        end
    endgenerate
  
    // Background plaette 
    rom_async #(
        .WIDTH(BG_COLOR_BITS),
        .DEPTH(64),
        .INIT_F("./image/palette.mem")
    ) bg_palette_rom (
        .addr(bg_idx),
        .data(bg_rom_data));

    // Block mask
    rom_async #(
        .WIDTH(2),
        .DEPTH(SPR_PIXELS),
        .INIT_F("./image/block.mem")
    ) block_rom (
        .addr(rom_addr),
        .data(block_rom_data));

    // BGM
    rom_async_hex #(
        .WIDTH(16),
        .DEPTH(47000),
        .INIT_F("./image/bgm_mem.hex")
    ) bgm_rom (
        .addr(bgm_rom_addr),
        .data(bgm_rom_data));

    logic [3:0] score[0:3];
    logic [1:0] speed;
    logic [2:0] block_state[0:21][0:9];
    logic [2:0] value, value_next;
    logic [15:0] offset, offset_next;
    logic [4:0] row;
    logic [3:0] col;
    logic [4:0] row_to_del;
    logic del_row, write_block, reset_sw, paused, over;
    logic [12:0] divider = 0;
    logic [12:0] interval[0:2];
    logic[23:0] block_color[0:7];
    
    /*logic[23:0] rand_block_color[0:7];
    int rand_val;*/

    initial begin
	int rand_val = 1;
        // Initialize block colors.
        block_color[0] = {8'h00, 8'h00, 8'h00};
        block_color[1] = {8'hd5, 8'h00, 8'h00};
        block_color[2] = {8'h4c, 8'haf, 8'h50};
        block_color[3] = {8'h9e, 8'h9e, 8'h9e};
        block_color[4] = {8'hff, 8'heb, 8'h3b};
        block_color[5] = {8'h03, 8'ha8, 8'hf4};
        block_color[6] = {8'hd5, 8'h00, 8'hf9};
        block_color[7] = {8'hff, 8'h98, 8'h00};
	
	// rand_block_color = block_color;
        // Initialize music intervals.

        interval[0] = 6250;
        interval[1] = 5000;
        interval[2] = 4000;

        // Initialize parameters
        speed = 1;
        for (int i = 0; i < 4; i++) score[i] = 4'd0;
        del_row = 0;
        write_block = 0;
        reset_sw = 0;
        paused = 0;
        
        for (int i = 0; i < 22; i++)
                for (int j = 0; j < 10; j++)
                    block_state[i][j] = 3'd0;
        // write a smile face
	block_state[7][3] = 4;
	block_state[7][4] = 4;
	block_state[7][5] = 4;
	block_state[7][6] = 4;
	block_state[8][2] = 4;
	block_state[8][7] = 4;
	block_state[9][1] = 4;
	block_state[9][8] = 4;
	block_state[10][1] = 4;
	block_state[10][8] = 4;
	block_state[11][1] = 4;
	block_state[11][8] = 4;
	block_state[12][1] = 4;
	block_state[12][8] = 4;
	block_state[13][1] = 4;
	block_state[13][8] = 4;
	block_state[14][2] = 4;
	block_state[14][7] = 4;
	block_state[15][3] = 4;
	block_state[15][4] = 4;
	block_state[15][5] = 4;
	block_state[15][6] = 4;

	// eyes
	block_state[9][3] = 3;
	block_state[9][6] = 3;
	
	// mouth
	block_state[12][3] = 1;
	block_state[12][6] = 1;
	block_state[13][5] = 1;
	block_state[13][4] = 1;


    end

    always_ff @(posedge clk) begin

        if (reset_sw) begin
            speed <= 1;
            for (int i = 0; i < 4; i++) score[i] <= 4'd0;
            for (int i = 0; i < 22; i++)
                for (int j = 0; j < 10; j++)
                    block_state[i][j] <= 3'd0;
            reset_sw <= 0;
        end
        else if (chipselect && write) begin
            case (address)
            3'h0: begin
                row <= writedata[15:11];
                col <= writedata[10:7]; 

                case (writedata[6:2])
                // I
                5'b00100, 5'b00110: offset <= 16'b0000000100100011;
                5'b00101, 5'b00111: offset <= 16'b0000010010001100;
                // O
                5'b01000, 5'b01001, 5'b01010, 5'b01011: offset <= 16'b0000010000010101;
                // T
                5'b01100: offset <= 16'b0000000100100101;
                5'b01101: offset <= 16'b0100000101011001;
                5'b01110: offset <= 16'b0001010001010110;
                5'b01111: offset <= 16'b0000010010000101;
                // L
                5'b10000: offset <= 16'b0000000100100100;
                5'b10001: offset <= 16'b0000000101011001;
                5'b10010: offset <= 16'b0010010001010110;
                5'b10011: offset <= 16'b0000010010001001;
                // J
                5'b10100: offset <= 16'b0000000100100110;
                5'b10101: offset <= 16'b1000000101011001;
                5'b10110: offset <= 16'b0000010001010110;
                5'b10111: offset <= 16'b0000010010000001;
                // Z
                5'b11000, 5'b11010: offset <= 16'b0000000101010110;
                5'b11001, 5'b11011: offset <= 16'b0001010001011000;
                //S
                5'b11100, 5'b11110: offset <= 16'b0001001001000101;
                5'b11101, 5'b11111: offset <= 16'b0000010001011001;
                endcase

                value <= {3{writedata[1]}} & writedata[6:4];
                write_block <= 1;
            end
            3'h1: begin
                row_to_del <= writedata[4:0];
                del_row <= 1;
            end
            3'h2: begin
                score[0] <= writedata[3:0];
                score[1] <= writedata[7:4];
                score[2] <= writedata[11:8];
                score[3] <= writedata[15:12];
            end
            3'h3: begin
                value_next <= writedata[2:0];

		/*for(int ii = 1; ii < 8; ii ++)
			rand_block_color[ii] = block_color[(ii + rand_val)%7 +1];
		rand_val ++;*/

                case (writedata[2:0])
                3'd1: offset_next <= 16'b0000000100100011;
                3'd2: offset_next <= 16'b0000010000010101;
                3'd3: offset_next <= 16'b0000000100100101;
                3'd4: offset_next <= 16'b0000000100100100;
                3'd5: offset_next <= 16'b0000000100100110;
                3'd6: offset_next <= 16'b0000000101010110;
                3'd7: offset_next <= 16'b0001001001000101;
                endcase
		
            end
            3'h4:
                speed <= writedata[1:0];
            3'h5:
                reset_sw <= 1;
            3'h6: begin
                paused <= writedata[0] + 1;
                over <= writedata[1];
            end
            endcase
        end

        if (write_block) begin
            // Update block state
            block_state[row + offset[15:14]][col + offset[13:12]] <= value;
            block_state[row + offset[11:10]][col + offset[9:8]] <= value;
            block_state[row + offset[7:6]][col + offset[5:4]] <= value;
            block_state[row + offset[3:2]][col + offset[1:0]] <= value;
            write_block <= 0;
        end
        if (del_row) begin
            // Delete row
            for (int i = 21; i >= 1; i--)
                if (i <= row_to_del) block_state[i] <= block_state[i - 1];
            for (int j = 0; j < 10; j++) block_state[0][j] <= 3'd0;
            del_row <= 0;
        end
        
        if (over) begin
            for (int i = 0; i < 22; i++)
                    for (int j = 0; j < 10; j++)
                        block_state[i][j] = 3'd0;
            // show over
	    // o
	    block_state[2][5] = 4;
	    block_state[3][4] = 4;
	    block_state[3][6] = 4;
	    block_state[4][6] = 4;
	    block_state[4][4] = 4;
	    block_state[5][5] = 4;

	    // v
	    block_state[7][4] = 4;
	    block_state[7][6] = 4;
	    block_state[8][4] = 4;     
	    block_state[8][6] = 4;
	    block_state[9][5] = 4;

	    // e
	    block_state[11][5] = 4;
	    block_state[12][4] = 4;
	    block_state[12][6] = 4;
	    block_state[13][4] = 4;
	    block_state[13][5] = 4;
	    block_state[14][4] = 4;
	    block_state[15][5] = 4;

	    // r
	    block_state[17][5] = 4;
	    block_state[17][6] = 4;
	    block_state[18][4] = 4;            
	    block_state[19][4] = 4;
	    block_state[20][4] = 4;

            divider <= 0;
            bgm_rom_addr <= 0;
            over <= 0;

        end

        // music
        if (divider < interval[speed - 1]) begin
            divider <= divider + paused;
            avalon_streaming_source_l_valid <= 0;
            avalon_streaming_source_r_valid <= 0;
        end
        else begin
            divider <= 0;
	    bgm_rom_addr <= bgm_rom_addr + paused;
	    if (bgm_rom_addr >= 47000)
		bgm_rom_addr <= 0;
		avalon_streaming_source_l_valid <= paused;
		avalon_streaming_source_r_valid <= paused;
		avalon_left <= bgm_rom_data;
		avalon_right <= bgm_rom_data;
	    end
        end

    always_comb begin
        rom_addr = {vcount[3:0], hcount[4:1]};
        bg_addr = {vcount[8:0], hcount[7:1]};
        bg_idx = bg_idx_rom_data[hcount[10:8]][5:0];
        {VGA_R, VGA_G, VGA_B} = {8'hff, 8'hff, 8'hff};
        if (VGA_BLANK_n) begin
            // bg
            {VGA_R, VGA_G, VGA_B} = bg_rom_data;
            // draw sprites
            //blocks
            if (hcount[10:5] >= 24 && hcount[10:5] <=33 && vcount[9:4] >= 4 && vcount[9:4] <= 25)
                {VGA_R, VGA_G, VGA_B} = { 3{block_rom_data[1], {7{block_rom_data[0]}}} } & block_color[block_state[vcount[9:4] - 4][hcount[8:5] - 24]];
            else if (hcount[10:5] >= 11 && hcount[10:5] <= 14)
                // score number
                if (vcount[9:4] == 14)
                    {VGA_R, VGA_G, VGA_B} = {3{digit_rom_data[score[33 - hcount[10:5]]], 5'd0}};
                // speed number
                else if(hcount[10:5] == 14 && vcount[9:4] == 10)
                    {VGA_R, VGA_G, VGA_B} = {3{digit_rom_data[speed], 5'd0}};
                // next blocks
                else if ((hcount[10:5] == 11 + offset_next[13:12] && vcount[9:4] == 19 + offset_next[15:14])
                    || (hcount[10:5] == 11 + offset_next[9:8] && vcount[9:4] == 19 + offset_next[11:10])
                    || (hcount[10:5] == 11 + offset_next[5:4] && vcount[9:4] == 19 + offset_next[7:6])
                    || (hcount[10:5] == 11 + offset_next[1:0] && vcount[9:4] == 19 + offset_next[3:2]))
                     // Set the next value block's color
                     {VGA_R, VGA_G, VGA_B} = {3{block_rom_data[1], {7{block_rom_data[0]}}}} & block_color[value_next];
        end
    end
endmodule

module vga_counters(
 input logic 	     clk50, reset,
 output logic [10:0] hcount,  // hcount[10:1] is pixel column
 output logic [9:0]  vcount,  // vcount[9:0] is pixel row
 output logic 	     VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n);

/*
 * 640 X 480 VGA timing for a 50 MHz clock: one pixel every other cycle
 *
 * HCOUNT 1599 0             1279       1599 0
 *             _______________              ________
 * ___________|    Video      |____________|  Video
 *
 *
 * |SYNC| BP |<-- HACTIVE -->|FP|SYNC| BP |<-- HACTIVE
 *       _______________________      _____________
 * |____|       VGA_HS          |____|
 */
   // Parameters for hcount
   parameter HACTIVE      = 11'd 1280,
             HFRONT_PORCH = 11'd 32,
             HSYNC        = 11'd 192,
             HBACK_PORCH  = 11'd 96,
             HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC +
                            HBACK_PORCH; // 1600

   // Parameters for vcount
   parameter VACTIVE      = 10'd 480,
             VFRONT_PORCH = 10'd 10,
             VSYNC        = 10'd 2,
             VBACK_PORCH  = 10'd 33,
             VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC +
                            VBACK_PORCH; // 525

   logic endOfLine;

   always_ff @(posedge clk50 or posedge reset)
     if (reset)          hcount <= 0;
     else if (endOfLine) hcount <= 0;
     else  	         hcount <= hcount + 11'd 1;

   assign endOfLine = hcount == HTOTAL - 1;

   logic endOfField;

   always_ff @(posedge clk50 or posedge reset)
     if (reset)          vcount <= 0;
     else if (endOfLine)
       if (endOfField)   vcount <= 0;
       else              vcount <= vcount + 10'd 1;

   assign endOfField = vcount == VTOTAL - 1;

   // Horizontal sync: from 0x520 to 0x5DF (0x57F)
   // 101 0010 0000 to 101 1101 1111
   assign VGA_HS = !( (hcount[10:8] == 3'b101) &
		      !(hcount[7:5] == 3'b111));
   assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

   assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused

   // Horizontal active: 0 to 1279     Vertical active: 0 to 479
   // 101 0000 0000  1280	       01 1110 0000  480
   // 110 0011 1111  1599	       10 0000 1100  524
   assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
			!( vcount[9] | (vcount[8:5] == 4'b1111) );

   /* VGA_CLK is 25 MHz
    *             __    __    __
    * clk50    __|  |__|  |__|
    *
    *             _____       __
    * hcount[0]__|     |_____|
    */
   assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive

endmodule

module rom_async #(
  parameter WIDTH=8,
  parameter DEPTH=256,
  parameter INIT_F="")
  (
    input wire logic [ADDRW-1:0] addr,
    output     logic [WIDTH-1:0] data
  );

  localparam ADDRW=$clog2(DEPTH);
  logic [WIDTH-1:0] memory [DEPTH];

  initial begin
    if (INIT_F != 0) begin
      $display("Creating rom_async from init file '%s'.", INIT_F);
      $readmemb(INIT_F, memory);
    end
  end

  always_comb data = memory[addr];
endmodule

module rom_async_hex #(
  parameter WIDTH=8,
  parameter DEPTH=256,
  parameter INIT_F="")
  (
    input wire logic [ADDRW-1:0] addr,
    output     logic [WIDTH-1:0] data
  );

  localparam ADDRW=$clog2(DEPTH);
  logic [WIDTH-1:0] memory [DEPTH];

  initial begin
    if (INIT_F != 0) begin
      $display("Creating rom_async from init file '%s'.", INIT_F);
      $readmemh(INIT_F, memory);
    end
  end

  always_comb data = memory[addr];
endmodule
