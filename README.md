# Conway's Game of Life for the Ledger Nano S

It's an app that plays Conway's Game of Life on the [Ledger Nano S hardware wallet](https://shop.ledger.com/products/ledger-nano-s). Features customizable speed and randomization to prevent stabilization.

## Building

You'll need an arm-none-eabi-gcc and a copy of clang placed at `$BOLOS_ENV/gcc/bin` and `$BOLOS_ENV/clang/bin`, respectively (on my computer, I make this work by symlinking `/opt/local` to `$BOLOS_ENV/gcc` and `/usr` to `$BOLUS_ENV/clang`). Then run `make load` to install the app on your device.
