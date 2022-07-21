#
# Bash completion definition for CliFM
#
# Author:
#   L. Abramovich <johndoe.arch@outlook.com>
#

_clifm ()
{
    COMPREPLY=()
    local IFS=$'\n'
    local cur=$2 prev=$3
    local -a opts
    opts=(
        -a
        --no-hidden
        -A
        --show-hidden
        -b
        --bookmarks-file
        -c
        --config-file
        -D
        --config-dir
        -e
        --no-eln
        -E
        --eln-use-workspace-color
        -f
        --no-dirs-first
        -F
        --dirs-first
        -g
        --pager
        -G
        --no-pager
        -h
        --help
		-H
		--horizontal-list
        -i
        --no-case-sensitive
        -I
        --case-sensitive
        -k
        --keybindings-file
        -l
        --no-long-view
        -L
        --long-view
        -m
        --dirhist-map
        -o
        --no-autols
        -O
        --autols
        -p
        --path
        -P
        --profile
        -r
        --no-refresh-on-empty-line
        -s
        --splash
        -S
        --stealth-mode
        -t
        --disk-usage-analyzer
        -u
        --no-unicode
        -U
        --unicode
        -v
        --version
        -w
        --workspace
        -x
        --no-ext-cmds
        -y
        --light-mode
        -z
        --sort
		--bell
        --case-sens-dirjump
        --case-sens-path-comp
        --cd-on-quit
        --color-scheme
        --cwd-in-title
        --dektop-notifications
        --disk-usage
        --enable-logs
        --expand-bookmarks
        --full-dir-size
        --fuzzy-match
        --icons
        --icons-use-file-color
        --list-and-quit
        --max-dirhist
        --max-files
        --max-path
        --no-dir-jumper
        --no-cd-auto
        --no-classify
        --no-clear-screen
        --no-color
        --no-columns
        --no-control-d-exit
        --no-file-cap
        --no-file-ext
        --no-files-counter
        --no-follow-symlink
        --no-highlight
        --no-history
        --no-open-auto
        --no-props-color
        --no-refresh-on-resize
        --no-restore-last-path
        --no-suggestions
        --no-tips
		--no-warning-prompt
        --no-welcome-message
        --only-dirs
        --open
        --opener
        --print-sel
        --rl-vi-mode
        --secure-cmds
        --secure-env
        --secure-env-full
        --share-selbox
        --sort-reverse
        --std-tab-comp
        --trash-as-rm
        --virtual-dir
        --virtual-dir-full-paths
    )

    if [[ $prev == "-b" || $prev == "-c" || $prev == "-k" || $prev == "-p" ]]; then
        COMPREPLY=( $(compgen -f -d -- "$cur") )

    elif [[ $prev == "-P" ]]; then
        local profiles=$(basename -a $(ls -Ad ~/.config/clifm/profiles/*))
        COMPREPLY=( $(compgen -W "$profiles" -- "$cur") )

    elif [[ $prev == "--color-scheme" ]]; then
        local schemes=$(basename -a $(ls -Ad ~/.config/clifm/colors/*) | cut -d"." -f1)
        COMPREPLY=( $(compgen -W "$schemes" -- "$cur") )

    elif [[ $prev == -z || $prev == "--sort" ]]; then
        local args=$(echo -e "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\m11")
        COMPREPLY=( $(compgen -W "$args" -- "$cur") )

    elif [[ $prev == "--bell" ]]; then
	    local args=(echo -e "0\n1\n2\n3")
	    COMPREPLY=( $(compgen -W "$args" -- "$cur") )

    elif [[ $prev == "--opener" ]]; then
	    local apps=$(ls -AG $(echo $PATH | awk -F':' '{ for (i=1; i<NF; i++) print $i}') | grep -v "/\|^$")
        COMPREPLY=( $(compgen -W "$apps" -- "$cur") )

    elif [[ $cur == -* ]]; then
        COMPREPLY=( $(compgen -W "${opts[*]}" -- "$cur") )

    else
        COMPREPLY=( $(compgen -f -d -- "$cur") )

    fi
}

complete -o filenames -F _clifm clifm
