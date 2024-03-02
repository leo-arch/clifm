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
        --show-hidden
        -A
        --no-hidden
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
        --dirs-first
        -F
        --no-dirs-first
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
        --long-view
        -L
        --follow-symlinks-long
        -m
        --dirhist-map
        -o
        --autols
        -O
        --no-autols
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
        -T
        --trash-dir
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
        --data-dir
        --dektop-notifications
        --disk-usage
        --full-dir-size
        --fuzzy-algo
        --fuzzy-matching
        --fzfpreview-hidden
        --fzftab
        --fzytab
        --icons
        --icons-use-file-color
        --int-vars
        --list-and-quit
        --lscolors
        --max-dirhist
        --max-files
        --max-path
        --mnt-udisk2
        --no-apparent-size
        --no-bold
        --no-cd-auto
        --no-classify
        --no-clear-screen
        --no-color
        --no-columns
        --no-dir-jumper
        --no-file-cap
        --no-file-ext
        --no-files-counter
        --no-follow-symlinks
        --no-fzfpreview
        --no-highlight
        --no-history
        --no-open-auto
        --no-refresh-on-resize
        --no-restore-last-path
        --no-suggestions
        --no-tips
        --no-trim-names
		--no-warning-prompt
        --no-welcome-message
        --only-dirs
        --open
        --opener
        --preview
        --print-sel
        --ptime-style
        --readonly
        --rl-vi-mode
        --secure-cmds
        --secure-env
        --secure-env-full
        --sel-file
        --share-selbox
        --shotgun-file
        --si
        --smenutab
        --sort-reverse
        --stat
        --stat-full
        --stdtab
        --time-style
        --trash-as-rm
        --virtual-dir
        --virtual-dir-full-paths
        --vt100
    )

    if [[ $prev == "-b" || $prev == "-c" || $prev == "-k" || $prev == "-p" || $prev == "--open" || $prev == "--preview" || $prev == "--shotgun-file" ]]; then
        COMPREPLY=( $(compgen -f -d -- "$cur") )

    elif [[ $prev == "-P" || $prev == "--profile" ]]; then
        local profiles=$(basename -a $(ls -Ad ~/.config/clifm/profiles/*))
        COMPREPLY=( $(compgen -W "$profiles" -- "$cur") )

    elif [[ $prev == "--color-scheme" ]]; then
        local schemes=$(basename -a $(ls -Ad ~/.config/clifm/colors/*) | cut -d"." -f1)
        COMPREPLY=( $(compgen -W "$schemes" -- "$cur") )

	elif [[ $prev == "--fuzzy-algo" ]]; then
		local args=$(echo -e "1\n2")
		COMPREPLY=( $(compgen -W "$args" -- "$cur") )

	elif [[ $prev == "--time-style" ]]; then
		local args=$(echo -e "default\nrelative\niso\nlong-iso\nfull-iso")
		COMPREPLY=( $(compgen -W "$args" -- "$cur") )

	elif [[ $prev == "--ptime-style" ]]; then
		local args=$(echo -e "default\niso\nlong-iso\nfull-iso\nfull-iso-nano")
		COMPREPLY=( $(compgen -W "$args" -- "$cur") )

    elif [[ $prev == "-z" || $prev == "--sort" ]]; then
		local args=$(echo -e "none\nname\nsize\natime\nbtime\nctime\nmtime\nversion\nextension\ninode\nowner\ngroup\nblocks")
        COMPREPLY=( $(compgen -W "$args" -- "$cur") )

    elif [[ $prev == "--bell" ]]; then
	    local args=$(echo -e "0\n1\n2\n3")
	    COMPREPLY=( $(compgen -W "$args" -- "$cur") )

    elif [[ $prev == "--opener" ]]; then
	    local apps=$(ls -AG $(echo $PATH | awk -F':' '{ for (i=1; i<NF; i++) print $i}') | grep -v "/\|^$")
        COMPREPLY=( $(compgen -W "$apps" -- "$cur") )

	elif [[ $prev == "-w" || $prev == "--workspace" ]]; then
		local args=$(echo -e "1\n2\n3\n4\n5\n6\n7\n8")
        COMPREPLY=( $(compgen -W "$args" -- "$cur") )

    elif [[ $cur == -* ]]; then
        COMPREPLY=( $(compgen -W "${opts[*]}" -- "$cur") )

    else
        COMPREPLY=( $(compgen -f -d -- "$cur") )

    fi
}

complete -o filenames -F _clifm clifm
