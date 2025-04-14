library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity cast is
    generic (
        INPUT_WIDTH  : integer := 9;
        OUTPUT_WIDTH : integer := 16
    );
    port (
        din  : in  std_logic_vector(INPUT_WIDTH - 1 downto 0);
        dout : out std_logic_vector(OUTPUT_WIDTH - 1 downto 0)
    );
end cast;

architecture Behavioral of cast is
begin
    process(din)
        variable tmp_in  : signed(INPUT_WIDTH - 1 downto 0);
        variable tmp_out : signed(OUTPUT_WIDTH - 1 downto 0);
    begin
        tmp_in  := signed(din);
        tmp_out := resize(tmp_in, OUTPUT_WIDTH);
        dout    <= std_logic_vector(tmp_out);
    end process;
end Behavioral;
