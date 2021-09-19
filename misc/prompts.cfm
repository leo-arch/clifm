# A few prompts for CliFM

# The prompt line is build using command substitution ($(cmd)), string
# literals and/or one or more of the following escape sequences:

# \xnn: The character whose hexadecimal code is nn.
# \e: Escape character
# \h: The hostname, up to the first dot (.)
# \u: The username
# \H: The full hostname
# \n: A newline character
# \r: A carriage return
# \a: A bell character
# \d: The date, in abbrevieted form (ex: 'Tue May 26')
# \s: The name of the shell (everything after the last slash) currently used
# by CliFM
# \t: The time, in 24-hour HH:MM:SS format
# \T: The time, in 12-hour HH:MM:SS format
# \@: The time, in 12-hour am/pm format
# \A: The time, in 24-hour HH:MM format
# \w: The full current working directory, with $HOME abbreviated with a tilde
# \W: The basename of $PWD, with $HOME abbreviated with a tilde
# \p: A mix of the two above, it abbreviates the current working directory 
# only if longer than PathMax (a value defined in the configuration file).
# \z: Exit code of the last executed command. :) if success and :( in case of 
# error
# \$ '#', if the effective user ID is 0, and '$' otherwise
# \nnn: The character whose ASCII code is the octal value nnn
# \\: A backslash
# \[: Begin a sequence of non-printing characters. This is mostly used to 
# add color to the prompt line
# \]: End a sequence of non-printing characters

# Unicode characters could be inserted by directly pasting the corresponding
# char, or by inserting its hex code ('echo -ne "paste_your_char" | hexdump -C')

# Default
#Prompt="[\[\e[0;36m\]\S\[\e[0m\]]\l \A \u:\H \[\e[00;36m\]\w\n\[\e[0m\]\z \[\e[0;34m\]\$\[\e[0m\] "

# Simple
#Prompt="[\u@\H] \w \$ "

# The following prompts are built using a patched Nerdfont

# Fireplace
#Prompt="\[\e[00;31m\]┏\[\e[00;31m\]\[\e[00;37;41m\]\A\[\e[00;31;43m\]\[\e[00;30;43m\] \u:\H\[\e[00;33;41m\]\[\e[00;37;41m\] \w\[\e[00;31m\]\[\e[0m\]\n\[\e[00;31m\]┗ "


# Cold winter
Prompt="\[\e[00;37;100m\] \A \[\e[00;90;46m\]  \[\e[0;30;46m\]\u:\H \[\e[0;36;100m\]  \[\e[00;37;100m\]\w \[\e[00;90;40m\] \n \[\e[1;90m\]\[\e[0m\] "

# Cold winter full
#Prompt="\[\e[0;30;46m\] [\S] \[\e[0;36;100m\]\[\e[00;37;100m\] \A \[\e[00;90;46m\]  \[\e[0;30;46m\]\u:\H \[\e[0;36;100m\]  \[\e[00;37;100m\]\w \[\e[00;90;40m\] \n <\z\[\e[00;90;40m\]> \$\[\e[0m\] "

# Spot
#Prompt="\[\e[00;38;5;0;48;5;178m\] \A \u:\H \w \[\e[00;38;5;178;48;5;0m\]\[\e[0;40m\]\n\[\e[0;38;5;254;48;5;53m\] \$ \[\e[0;38;5;53;48;5;0m\] \[\e[0m\] "

# Artic particles
#Prompt="\[\e[00;37;44m\] \A \[\e[00;34;47m\]  \u:\H \[\e[00;37;44m\] \w \[\e[00;34;40m\] \n\[\e[00;37;44m\] \$ \[\e[00;34;40m\] "

# Green Beret
#Prompt=" ╭─\[\e[0;38;5;239;48;5;232m\]\[\e[0;38;5;15;48;5;239m\]  \A \[\e[0;38;5;239;48;5;70m\]\[\e[0;38;5;0;48;5;70m\] \w \[\e[0;38;5;70;48;5;232m\]\n \[\e[0;40m\]╰──\[\e[0;38;5;70;48;5;0m\]▸\[\e[0;40m\] "
