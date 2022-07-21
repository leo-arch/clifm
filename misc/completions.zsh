#compdef clifm
#
# Completion definition for clifm
# Author: L. Abramovich
#

setopt localoptions noshwordsplit noksharrays
local -a args

args=(
	{-a,--no-hidden}'[do not show hidden files (default)]'
	{-A,--show-hidden}'[show hidden files]'
	{-b+,--bookmarks-file=}'[specify an alternative bookmarks file]:filename:_files'
	{-c+,--config-file=}'[specify an alternative configuration file]:filename:_files'
	{-D+,--config-dir=}'[specify an alternative configuration directory]:directory:_directories'
	{-e,--no-eln}'[do not print ELN (entry list number) at the left of each filename]'
	{-E,--eln-use-workspace-color}'[ELN'\''s use the color of the current workspace]'
	{-f,--no-dirs-first}'[do not list directories first]'
	{-F,--dirs-first}'[list directories first (default)]'
	{-g,--pager}'[enable the pager]'
	{-G,--no-pager}'[disable the pager (default)]'
	{-h,--help}'[show this help and exit]'
	{-H,--horizontal-list}'[list files horizontally]'
	{-i,--no-case-sensitive}'[no case-sensitive files listing (default)]'
	{-I,--case-sensitive}'[case-sensitive files listing]'
	{-k+,--keybindings-file=}'[specify an alternative keybindings file]:filename:_files'
	{-l,--no-long-view}'[disable long view mode (default)]'
	{-L,--long-view}'[enable long/detailed view mode]'
	{-m,--dihist-map}'[enable the directory history map]'
	{-o,--no-autols}'['\''cd'\'' works as the shell '\''cd'\'' command]'
	{-O,--autols}'['\''cd'\'' lists files on the fly (default)]'
	{-p+,--path=}'[use PATH as CliFM starting path]:directory:_directories'
	{-P+,--profile=}'[use (or create) PROFILE as profile]:profile:->profiles'
	{-r,--no-refresh-on-empty-line}'[do not refresh the screen when pressing Enter on empty line]'
	{-s,--splash}'[print the logo screen at startup]'
	{-S,--stealth-mode}'[leave no trace on the host system]'
	{-t,--disk-usage-analyzer}'[run in disk usage analyzer mode]'
	{-u,--no-unicode}'[disable unicode]'
	{-U,--unicode}'[enable unicode]'
	{-v,--version}'[show version details and exit]'
	{-w+,--workspace=}'[start in workspace NUM]:workspace:->workspaces'
	{-x,--no-ext-cmds}'[disallow the use of external commands]'
	{-y,--light-mode}'[enable the light mode]'
	{-z+,--sort=}'[sort files by METHOD]:method:->methods'
	'--bell[set terminal bell style: 0=none; 1=audible; 2=viusal (default); 3=flash]:bell:->bells'
	'--case-sens-dirjump[do not ignore case when consulting the jump database]'
	'--case-sens-path-comp[do not ignore case when completing file names]'
	'--cd-on-quit[write last visited path to $XDG_CONFIG_HOME/clifm/.last to be accessed later by a shell funtion]'
	'--color-scheme=[set color scheme]:color:->colorschemes'
	'--cwd-in-title[print current directory in the terminal window title]'
	'--desktop-notifications[enable desktop notifications]'
	'--disk-usage[show disk usage (free/total) for the filesystem to which the current directory belongs]'
	'--enable-logs[enable program logs]'
	'--expand-bookmarks[expand bookmark names into the corresponding bookmark paths]'
	'--full-dir-size[print size of directories and their contents in long view mode]'
	'--fuzzy-match[enable fuzzy matches for completions and suggestions (only file names and paths)]'
	'--icons[enable icons]'
	'--icons-use-file-color[icons color follows file color]'
	'--int-vars[enable internal variables]'
	'--list-and-quit[list files and quit]'
	'--max-dirhist=[maximum number of visited directories to remember]:int:'
	'--max-files=[list only up to NUM files]:int:'
	'--max-path=[set the maximun number of characters of the prompt path]:int:'
	'--no-dir-jumper[disable the directory jumper function]'
	'--no-cd-auto[force the use of '\''cd'\'' to change directories]'
	'--no-classify[Do not append filetype indicators]'
	'--no-clear-screen[do not clear the screen when listing directories]'
	'--no-colors[disable filetype colors for files listing]'
	'--no-columns[disable columned files listing]'
	'--no-control-d-exit[do not allow exit on EOF (Control-d)]'
	'--no-file-cap[do not check files capabilities when listing files]'
	'--no-file-ext[do not check files extension when listing files]'
	'--no-files-counter[disable the files counter for directories]'
	'--no-follow-symlink[do not follow symbolic links when listing files]'
	'--no-highlight[disable syntax highlighting]'
	'--no-history[do not write commands into the history file]'
	'--no-open-auto[same as no-cd-auto, but for files]'
    '--no-props-color[do not colorize files properties]'
    '--no-refresh-on-resize[do not update the files list on window'\''s resize]'
    '--no-restore-last-path[do not restore last visited directory at startup]'
	'--no-suggestions[disable auto-suggestions]'
	'--no-tips[disable startup tips]'
	'--no-warning-prompt[disable the warning prompt, used to highlight invalid command names]'
	'--no-welcome-message[disable the welcome message]'
	'--only-dirs[list only directories and symbolic links to directories]'
	'--open[run as a stand-alone resource opener: open FILE and exit]'
	'--opener=[resource opener to use instead of '\''Lira'\'', CliFM built-in opener]:opener:_command_names'
	'--print-sel[always print the list of selected files]'
	'--rl-vi-mode[set readline to vi editing mode (defaults to emacs editing mode)]'
	'--secure-cmds[filter commands to prevent command injection]'
	'--secure-env[run in a sanitized environment (regular mode)]'
	'--secure-env-full[run in a sanitized environment (full mode)]'
	'--share-selbox[make the Selection Box common to different profiles]'
	'--sort-reverse[sort in reverse order]'
	'--std-tab-comp[use standard TAB completion]'
	'--trash-as-rm[the '\''r'\'' command executes '\''trash'\'' instead of '\''rm'\'']'
	'--virtual-dir=[use PATH as CliFM virtual directory]:directory:_directories'
	'--virtual-dir-full-paths[print full path file names in virtual directories]'
	'*:filename:_files'
)

_arguments -w -s -S $args[@] && ret=0

case "$state" in
	profiles)
		local -a prof_files
		prof_files=( $(basename -a $HOME/.config/clifm/profiles/*) )
		_multi_parts / prof_files
	;;
	bells)
		_values -s , 'bells' 0 1 2 3
	;;
	colorschemes)
		local -a color_schemes
		color_schemes=( $(basename -a $HOME/.config/clifm/colors/* | cut -d. -f1) )
		_multi_parts / color_schemes
	;;
	methods)
		_values -s , 'methods' 0 1 2 3 4 5 6 7 8 9 10
	;;
	workspaces)
		_values -s , 'workspaces' 1 2 3 4 5 6 7 8
	;;
esac
