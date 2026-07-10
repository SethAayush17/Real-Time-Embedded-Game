# Real-Time-Embedded-Game

A fully functional real-time dodge game built in C on an MSP432 microcontroller (ARM Cortex-M4F), rendered on a 128x128 LCD display and controlled via analog joystick and pushbuttons. Built for ECE 2564 at Virginia Tech.

**Gameplay**

The game opens with a title screen before transitioning to a main menu with three options — Play, Instructions, and High Scores — navigated using pushbuttons BB1 and BB2. During gameplay, the player controls a character using the analog joystick, shooting bullets with BB1 to damage the enemy while dodging incoming projectiles. The enemy moves left to right across the screen and cycles through four distinct attack patterns: vertical rain, horizontal rain from the left, diagonal from corners, and horizontal rain from both sides simultaneously. The first player to reach zero HP loses. The final score is calculated as the player's remaining HP minus time spent in the game, and the top five scores are stored persistently in the high scores screen.

**Hardware Architecture**

The system runs on the MSP432 LaunchPad development board with the following peripherals:

128x128 LCD — connected via SPI (eUSCI_B) for all graphics rendering including menus, gameplay, and game over screens
Analog Joystick — read through the onboard Precision ADC, converting analog X/Y voltages to player pixel coordinates
Pushbuttons BB1 and BB2 — connected to GPIO pins P3.5 and P5.1, used for menu navigation and shooting
Timer32 — used for title screen countdown and elapsed game time tracking
NVIC — manages interrupts for button inputs and ADC readings, enabling the non-blocking architecture
LEDs — used for non-blocking verification testing

**Software Architecture**

The entire application is built around a non-blocking super-loop — all game logic is implemented using if statements rather than while loops, ensuring no single operation ever stalls the processor. This is critical for maintaining responsive joystick input, smooth bullet movement, and simultaneous enemy AI across all four attack patterns without any system freezing.

The codebase is fully modular — no function exceeds 50 lines of code, with each subsystem (player movement, enemy AI, bullet logic, collision detection, LCD rendering, menu navigation) broken into its own function called from the main game loop. A hardware abstraction layer (HAL) cleanly separates all hardware interactions (GPIO reads, ADC conversions, SPI writes) from application logic, making the code portable and straightforward to debug. All magic numbers are replaced with #defines and named constants defined at the top of the header file.

Tools & Languages: C, Code Composer Studio, MSP432 LaunchPad, DriverLib HAL
