# This file is part of CliFM

# A few example prompts for CliFM

# The regular prompt (just as the warning one) is built using command
# substitution ($(cmd)), string literals and/or one or more of the
# following escape sequences:

# \xnn: The character whose hexadecimal code is nn.
# \e: Escape character
# \s: The name of the shell (everything after the last slash) currently
#used by CliFM
# \u: The username
# \H: The full hostname
# \h: The hostname, up to the first dot (.)
# \P: The current profile name
# \S: The current workspace number
# \l: The current mode (print an 'L' when running in light mode)
# \n: A newline character
# \r: A carriage return
# \a: A bell character
# \d: The date, in abbrevieted form (ex: 'Tue May 26')
# \t: The time, in 24-hour HH:MM:SS format
# \T: The time, in 12-hour HH:MM:SS format
# \@: The time, in 12-hour am/pm format
# \A: The time, in 24-hour HH:MM format
# \w: The full current working directory, with $HOME abbreviated with a
#tilde
# \W: The basename of $PWD, with $HOME abbreviated with a tilde
# \p: A mix of the two above: it abbreviates the current working directory 
#only if longer than PathMax (a value defined in the configuration file).
# \z: Exit code of the last executed command. :) if success and :( in case
#of error
# \$ '#', if the effective user ID is 0, and '$' otherwise
# \nnn: The character whose ASCII code is the octal value nnn
# \\: A backslash
# \[: Begin a sequence of non-printing characters. This is mostly used to 
#add color to the prompt line
# \]: End a sequence of non-printing characters

# Unicode characters could be inserted by directly pasting the
# corresponding char, or by inserting its hex code
# ('echo -ne "paste_your_char" | hexdump -C')

# To use any of the below prompts just copy the corresponding lines to the
# configuration file

# If using a non-default prompt, you might want to set 'PromptStyle' in the
# configuration file to 'custom' to prevent the automatic insertion of
# workspace number, last exit code, stealth mode, trash, and selected files
# indicators, etc.
# This information however is available as environment variables. Constult
# the manpage for more information

### Default ###
#Prompt="\[\e[0m\][\[\e[0;36m\]\S\[\e[0m\]]\l \A \u:\H \[\e[0;36m\]\w\n\[\e[0m\]<\z\[\e[0m\]> \[\e[0;34m\]\$ \[\e[0m\]"
#WarningPromptStr="\[\e[00;02;31m\]\$(!) > "

### Default (colorless) ###
#Prompt="\[\e[0m\][\S]\l \A \u:\H \w\n<\z\[\e[0m\]> \$ "
#WarningPromptStr="(!) > "

### Default (plus some box-drawing characters) ###
#Prompt="\[\e[0;36m\]\[\e(0\]lq\[\e(B\]\[\e[0m\][\[\e[0;36m\]\S\[\e[0m\]]\l \A \u:\H \[\e[0;36m\]\w\n\[\e[0;36m\]\[\e(0\]mq\[\e(B\]\[\e[0m\]<\z\[\e[0m\]> \[\e[0;34m\]\$ \[\e[0m\]"
#WarningPromptStr="\[\e[0;36m\]\[\e(0\]mq\[\e(B\]\[\e[0m\]<\z\[\e[0m\]> \[\e[1;31m\]\! \[\e[00;02;31m\]"

### Classic ###
#Prompt="\[\e[1;32m\][\u@\H] \[\e[1;34m\]\w \[\e[0m\]\$ "
#WarningPromptStr="\[\e[1;32m\][\u@\H] \[\e[1;34m\]\w \[\e[1;31m\]! \[\e[00;02;31m\]"

### Curves ###
#Prompt="\[\e[01;32m\]╭─\[\e[0m\][\S]\[\e[01;32m\]─\[\e[0m\](\u:\H)\[\e[01;32m\]─\[\e[0m\][\[\e[00;36m\]\w\[\e[0m\]]\n\[\e[01;32m\]╰─\[\e[1;0m\]<\z\[\e[0m\]> \[\e[01;34m\]\λ\[\e[0m\] "
#WarningPromptStr="\[\e[01;32m\]╰─\[\e[1;0m\]<\z\[\e[0m\]> \[\e[01;31m\]\x\[\e[00;02;31m\] "

### Fireplace ###
#Prompt="\[\e[01;38;5;124m\]╭─\[\e[00;38;5;124m\]\[\e[00;37;48;5;124m\]\A \[\e[00;38;5;124;43m\]\[\e[00;30;43m\] \u:\H \[\e[00;33;48;5;124m\]\[\e[00;37;48;5;124m\] \w \[\e[00;38;5;124m\]\[\e[0m\]\n\[\e[01;38;5;124m\]╰─\[\e[0m\] "
#WarningPromptStr="\[\e[01;38;5;124m\]╰──\[\e[0;38;5;124m\] \[\e[00;02;31m\]"

### Cold winter ###
#Prompt="\[\e[00;37;100m\] \A \[\e[00;90;46m\]  \[\e[0;30;46m\]\u:\H \[\e[0;36;100m\]  \[\e[00;37;100m\]\w \[\e[00;90;40m\] \n \[\e[1;90m\]\[\e[0m\] "
#WarningPromptStr=" \[\e[0;36m\] \[\e[00;02;31m\]"

### Spot ###
#Prompt="\[\e[00;38;5;0;48;5;178m\] \A \u:\H \w \[\e[00;38;5;178;48;5;0m\]\[\e[0;40m\]\n\[\e[0;38;5;254;48;5;53m\] \$ \[\e[0;38;5;53;48;5;0m\] \[\e[0m\] "
#WarningPromptStr="\n\[\e[0;37;48;5;124m\] \x \[\e[0;38;5;124;48;5;0m\] \[\e[0m\] "

### Artic particles ###
#Prompt="\[\e[00;37;48;5;18m\] \A \[\e[00;38;5;18;47m\]  \u:\H \[\e[00;37;48;5;18m\] \w \[\e[00;38;5;18;40m\] \n\[\e[00;37;48;5;18m\] \$ \[\e[00;38;5;18;40m\] "
#WarningPromptStr="\[\e[00;02;31;47m\] \$ \[\e[00;37;0m\] \[\e[00;02;31m\]"

### Green Beret ###
#Prompt=" ╭─\[\e[0;38;5;239;48;5;0m\]\[\e[0;38;5;15;48;5;239m\]  \A \[\e[0;38;5;239;48;5;70m\]\[\e[0;38;5;0;48;5;70m\] \w \[\e[0;38;5;70;48;5;0m\]\n \[\e[0;40m\]╰──\[\e[0;38;5;70;48;5;0m\]▸\[\e[0;40m\] "
#WarningPromptStr="\[\e[0;40m\] ╰──\[\e[0;38;5;9;48;5;0m\]▸\[\e[00;02;31m\] "
