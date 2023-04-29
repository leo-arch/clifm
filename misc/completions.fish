#
# Fish completion definition for clifm.
#
# Author:
#   L. Abramovich <leo.clifm@outlook.com>
#

complete -c clifm -s a -l no-hidden -d 'Do not show hidden files'
complete -c clifm -s A -l show-hidden -d 'Show hidden files'

complete -c clifm -s b -l bookmarks-file -r -d 'Set an alternative bookmarks file'
complete -c clifm -s c -l config-file -r -d 'Set an alternative configuration file'
complete -c clifm -s D -l config-dir -r -d 'Set an alternative configuration directory'

complete -c clifm -s e -l no-eln -d 'Do not print ELN\'s (entry list number)'
complete -c clifm -s E -l eln-use-workspace-color -d 'ELN\'s use current workspace color'
complete -c clifm -s f -l no-dirs-first -d 'Do not list directories first'
complete -c clifm -s F -l dirs-first -d 'List directories first'
complete -c clifm -s g -l pager -d 'Enable the pager'
complete -c clifm -s G -l no-pager -d 'Disable the pager'
complete -c clifm -s h -l help -d 'Show help and exit'
complete -c clifm -s H -l horizontal-list -d 'List files horizontally'
complete -c clifm -s i -l no-case-sensitive -d 'No case-sensitive files listing'
complete -c clifm -s I -l case-sensitive -d 'Case-sensitive files listing'

complete -c clifm -s k -l keybindings-file -r -d 'Set an alternative keybindings file'

complete -c clifm -s l -l no-long-view -d 'Disable long/detail view mode'
complete -c clifm -s L -l long-view -d 'Enable long/detail view mode'
complete -c clifm -s m -l dirhist-map -d 'Enable the directory history map'
complete -c clifm -s o -l no-autols -d 'Do not list files automatically'
complete -c clifm -s O -l autols -d 'List files automatically'

complete -c clifm -s p -l path -r -d 'Set starting path'
complete -c clifm -s P -l profile -r -d 'Set profile' -x -a '(ls ~/.config/clifm/profiles/)'

complete -c clifm -s r -l no-refresh-on-empty-line -d 'Do not refresh the screen when pressing Enter on an empty line'
complete -c clifm -s s -l splash -d 'Enable the splash screen'
complete -c clifm -s S -l stealth-mode -d 'Run in incognito/private mode'
complete -c clifm -s t -l disk-usage-analyzer -d 'Run in disk usage analyzer mode'
complete -c clifm -s v -l version -d 'Show version details and exit'

complete -c clifm -s w -l workspace -r -d 'Set the starting workspace' -x -a '1 2 3 4 5 6 7 8'

complete -c clifm -s x -l no-ext-cmds -d 'Disallow the use of external commands'
complete -c clifm -s y -l light-mode -d 'Run in light mode'

complete -c clifm -s z -l sort -r -d 'Set sorting order' -x -a 'none name size atime btime ctime mtime version extension inode owner group'

complete -c clifm -l bell -r -d 'Set the terminal bell style' -x -a '0 1 2 3'
complete -c clifm -l case-sens-dirjump -d 'Do not ignore case when consulting the jump database'
complete -c clifm -l case-sens-path-comp -d 'Enable case sensitive path completion'
complete -c clifm -l cd-on-quit -d 'Enable cd-on-quit (consult the manpage)'

complete -c clifm -l color-scheme -r -d 'Set a color scheme' -x -a '(ls ~/.config/clifm/colors/*.clifm | awk -F\'/\' \'{print $NF}\' | cut -d\'.\' -f1)'
complete -c clifm -l cwd-in-title -d 'Print current directory in window\'s title'
complete -c clifm -l data-dir -r -d "Set an alternative data directory"

complete -c clifm -l desktop-notifications -d 'Enable desktop notifications'
complete -c clifm -l disk-usage -d 'Show disk usage (free/total)'
complete -c clifm -l full-dir-size -d 'Print full directories size (long view)'
complete -c clifm -l fuzzy-algo -r -d 'Select the algorithm used for fuzzy matching' -x -a '1 2'
complete -c clifm -l fuzzy-matching -d 'Enable fuzzy TAB completion/suggestions for file names and paths'
complete -c clifm -l fzfpreview-hidden -d 'Enable file previews (fzf only) with preview window hidden (toggle with Alt-p)'
complete -c clifm -l fzftab -d 'Use fzf to display completion matches (default if fzf is found)'
complete -c clifm -l fzytab -d 'Use fzy to display completion matches'
complete -c clifm -l icons -d 'Enable icons'
complete -c clifm -l icons-use-file-color -d 'Icon colors follow file colors'
complete -c clifm -l int-vars -d 'Enable internal variables'
complete -c clifm -l list-and-quit -d 'List files and quit'

complete -c clifm -l max-dirhist -r -d 'Maximum number of visited directories to recall'
complete -c clifm -l max-files -r -d 'Set the maximum number of listed files on screen'
complete -c clifm -l max-path -r -d 'Abbreviate the current directory in the prompt to its basename after NUM characters (if \\z is used in the prompt)'
complete -c clifm -l mnt-udisks2 -d 'Use \'udisks2\' instead of \'udevil\' for the \'media\' command'

complete -c clifm -l no-apparent-size -d 'Inform file sizes as used blocks instead of used bytes (apparent size)'
complete -c clifm -l no-bold -d 'Disable bold colors'
complete -c clifm -l no-cd-auto -d 'Disable the autocd function'
complete -c clifm -l no-classify -d 'Do not append file type indicators'
complete -c clifm -l no-clear-screen -d 'Do not clear the screen when listing files'
complete -c clifm -l no-color -d 'Disable colors'
complete -c clifm -l no-columns -d 'Disable columned files listing'
complete -c clifm -l no-dir-jumper -d 'Disable the directory jumper'
complete -c clifm -l no-file-cap -d 'Do not check file capabilities when listing files'
complete -c clifm -l no-file-ext -d 'Do not check file extensions when listing files'
complete -c clifm -l no-files-counter -d 'Disable the files counter for directories'
complete -c clifm -l no-follow-symlinks -d 'Do not follow symbolic links when listing files'
complete -c clifm -l no-fzfpreview -d 'Disable file preview for TAB completion (fzf mode only)'
complete -c clifm -l no-highlight -d 'Disable syntax highlighting'
complete -c clifm -l no-history -d 'Do not write command into the history file'
complete -c clifm -l no-open-auto -d 'Same as no-cd-auto, but for files'
complete -c clifm -l no-refresh-on-resize -d 'Do not update the files list upon window\'s resize'
complete -c clifm -l no-restore-last-path -d 'Do not record the last visited directory'
complete -c clifm -l no-suggestions -d 'Disable auto-suggestions'
complete -c clifm -l no-tips -d 'Disable startup tips'
complete -c clifm -l no-trim-names -d 'Do not trim file names'
complete -c clifm -l no-warning-prompt -d 'Disable the warning prompt'
complete -c clifm -l no-welcome-message -d 'Disable the welcome message'
complete -c clifm -l only-dirs -d 'List only directories and symbolic links to directories'

complete -c clifm -l open -r -d 'Open FILE (via Lira) and exit'
complete -c clifm -l opener -r -d 'Use APP as resource opener'
complete -c clifm -l preview -r -d 'Display a preview of FILE (via shotgun) and exit'

complete -c clifm -l print-sel -d 'Keep the list of selected files in sight'
complete -c clifm -l rl-vi-mode -d 'Set readline to vi editing mode (defaults to emacs mode)'
complete -c clifm -l secure-cmds -d 'Filter commands to prevent command injection'
complete -c clifm -l secure-env -d 'Run in a sanitized environment (regular mode)'
complete -c clifm -l secure-env-full -d 'Run in a sanitized environment (full mode)'

complete -c clifm -l sel-file -r -d 'Set a custom selections file'

complete -c clifm -l share-selbox -d 'Make the Selection Box common to different profiles'

complete -c clifm -l shotgun-file -r -d 'Set a custom configuration file for shotgun'

complete -c clifm -l si -d 'Print sizes in powers of 1000 instead of 1024'
complete -c clifm -l smenutab -d 'Use smenu to display completion matches'
complete -c clifm -l sort-reverse -d 'Sort in reverse order, e.g. z-a instead of a-z'
complete -c clifm -l stdtab -d 'Force the use of the standard TAB completion mode (readline)'
complete -c clifm -l trash-as-rm -d 'The \'r\' command executes \'trash\' instead of \'rm\' to prevent accidental deletions'
complete -c clifm -l virtual-dir-full-paths -d 'Files in virtual directories are listed as full paths instead of target base names'

complete -c clifm -l virtual-dir -r -d 'Absolute path to a directory to be used as virtual directory'
complete -c clifm -l vt100 -d 'Run in VT100 compatibility mode'