#compdef clifm
#
# Completion definition for clifm
# Author: L. Abramovich
#

setopt localoptions noshwordsplit noksharrays
local -a args

args=(
	{-a,--show-hidden}'[show hidden files]'
	{-A,--no-hidden}'[do not show hidden files (default)]'
	{-b+,--bookmarks-file=}'[specify an alternative bookmarks file]:filename:_files'
	{-c+,--config-file=}'[specify an alternative configuration file]:filename:_files'
	{-D+,--config-dir=}'[specify an alternative configuration directory]:directory:_directories'
	{-e,--no-eln}'[do not print ELNs (entry list number) at the left of each filename]'
	{-E,--eln-use-workspace-color}'[ELNs use the color of the current workspace]'
	{-f,--dirs-first}'[list directories first (default)]'
	{-F,--no-dirs-first}'[do not list directories first]'
	{-g,--pager}'[enable the pager]'
	{-G,--no-pager}'[disable the pager (default)]'
	{-h,--help}'[show this help and exit]'
	{-H,--horizontal-list}'[list files horizontally]'
	{-i,--no-case-sensitive}'[no case-sensitive files listing (default)]'
	{-I,--case-sensitive}'[case-sensitive files listing]'
	{-k+,--keybindings-file=}'[specify an alternative keybindings file]:filename:_files'
	{-l,--long-view}'[enable long/detailed view mode]'
	{-L,--follow-symlinks-long}'[follow symbolic links in long view]'
	{-m,--dihist-map}'[enable the directory history map]'
	{-o,--autols}'['\''cd'\'' lists files on the fly (default)]'
	{-O,--no-autols}'['\''cd'\'' works as the shell '\''cd'\'' command]'
	{-p+,--path=}'[set the starting path]:directory:_directories'
	{-P+,--profile=}'[use/create PROFILE as profile]:profile:->profiles'
	{-r,--no-refresh-on-empty-line}'[do not refresh the screen when pressing Enter on empty line]'
	{-s,--splash}'[print the logo screen at startup]'
	{-S,--stealth-mode}'[leave no trace on the host system]'
	{-t,--disk-usage-analyzer}'[run in disk usage analyzer mode]'
	{-T+,--trash-dir=}'[set an alternative trash directory]:directory:_directories'
	{-v,--version}'[show version details and exit]'
	{-w+,--workspace=}'[start in workspace NUM]:workspace:->workspaces'
	{-x,--no-ext-cmds}'[disallow the use of external commands]'
	{-y,--light-mode}'[enable the light mode]'
	{-z+,--sort=}'[sort files by METHOD]:method:->methods'
	'--bell=[set terminal bell style: 0=none; 1=audible; 2=viusal (default); 3=flash]:bell:->bells'
	'--case-sens-dirjump[do not ignore case when consulting the jump database]'
	'--case-sens-path-comp[do not ignore case when completing file names]'
	'--cd-on-quit[write last visited path to $XDG_CONFIG_HOME/clifm/.last to be accessed later by a shell funtion]'
	'--color-scheme=[set color scheme]:color:->colorschemes'
	'--color-links-as-target[colorize symbolic links using the target file color]'
	'--cwd-in-title[print current directory in the terminal window title]'
	'--data-dir=[use PATH as data directory]:directory:_directories'
	'--desktop-notifications[enable desktop notifications]'
	'--disk-usage[show disk usage (free/total) for the filesystem to which the current directory belongs]'
	'--full-dir-size[print size of directories and their contents in long view mode]'
	'--fuzzy-algo=[fuzzy matching algorithm]:algo:->algos'
	'--fuzzy-matching[enable fuzzy matches for filename/path completions and suggestions]'
	'--fzfpreview-hidden[same as --fzftab, but with the preview window hidden. Toggle it via Alt-p]'
	'--fzftab[use FZF to display completion matches]'
	'--fzytab[use FZY to display completion matches]'
	'--icons[enable icons]'
	'--icons-use-file-color[icons color follows file color]'
	'--int-vars[enable internal variables]'
	'--list-and-quit[list files and quit]'
	'--lscolors[read file colors from LS_COLORS]'
	'--max-dirhist=[maximum number of visited directories to remember]:int:'
	'--max-files=[list only up to NUM files]:int:'
	'--max-path=[set the maximun number of characters of the prompt path]:int:'
	'--mimelist-file=[set Lira configuration file to FILE]:filename:_files'
	'--mnt-udisks2[use udisks2 instead of udevil for the media command]'
	'--no-apparent-size[print file sizes as used blocks instead of used bytes]'
	'--no-bold[disable bold colors]'
	'--no-cd-auto[force the use of '\''cd'\'' to change directories]'
	'--no-classify[Do not append filetype indicators]'
	'--no-clear-screen[do not clear the screen when listing directories]'
	'--no-color[Run without colors]'
	'--no-columns[disable columned files listing]'
	'--no-dir-jumper[disable the directory jumper function]'
	'--no-file-cap[do not check files capabilities when listing files]'
	'--no-file-ext[do not check files extension when listing files]'
	'--no-files-counter[disable the files counter for directories]'
	'--no-follow-symlinks[do not follow symbolic links when listing files]'
	'--no-fzfpreview[disable file previews for tab completion (fzf mode only)]'
	'--no-highlight[disable syntax highlighting]'
	'--no-history[do not write commands into the history file]'
	'--no-open-auto[same as no-cd-auto, but for files]'
    '--no-refresh-on-resize[do not update the files list upon window'\''s resize]'
    '--no-report-cwd[do not report the working directory to the terminal]'
    '--no-restore-last-path[do not restore last visited directory at startup]'
	'--no-suggestions[disable auto-suggestions]'
	'--no-tips[disable startup tips]'
	'--no-trim-names[do not trim file names]'
	'--no-unicode[do not use Unicode characters]'
	'--no-warning-prompt[disable the warning prompt, used to highlight invalid command names]'
	'--no-welcome-message[disable the welcome message]'
	'--only-dirs[list only directories and symbolic links to directories]'
	'--open=[open FILE and exit]:filename:_files'
	'--opener=[resource opener to use instead of '\''Lira'\'', CliFM built-in opener]:opener:_command_names'
	'--pager-view=[how to list files in the pager: auto (default), long, short]:pager_view:->pager_views'
	'--physical-size[same as --no-apparent-size]'
	'--preview=[display a preview of FILE and exit]:filename:_files'
	'--print-sel[always print the list of selected files]'
	'--ptime-style=[time/date style used by the p/pp command]:pstyle:->pstyles'
	'--readonly[disable internal commands able to modify the file system]'
	'--rl-vi-mode[set readline to vi editing mode (defaults to emacs editing mode)]'
	'--secure-cmds[filter commands to prevent command injection]'
	'--secure-env[run in a sanitized environment (regular mode)]'
	'--secure-env-full[run in a sanitized environment (full mode)]'
	'--sel-file=[set FILE as selections file]:filename:_files'
	'--share-selbox[make the Selection Box common to different profiles]'
	'--shotgun-file=[set shotgun configuration file to FILE]:filename:_files'
	'--si[print sizes in powers of 1000 instead of 1024]'
	'--smenutab[use smenu to display completion matches]'
	'--sort-reverse[sort in reverse order]'
	'--stat=[run the '\''p'\'' command on FILE and exit]:filename:_files'
	'--stat-full=[run the '\''pp'\'' command on FILE and exit]:filename:_files'
	'--stdtab[use standard tab completion]'
	'--time-style=[time/date style used in long view]:style:->styles'
	'--trash-as-rm[the '\''r'\'' command executes '\''trash'\'' instead of '\''rm'\'']'
	'--unicode[force the use of Unicode decorations]'
	'--virtual-dir=[use PATH as CliFM virtual directory]:directory:_directories'
	'--virtual-dir-full-paths[print full path file names in virtual directories]'
	'--vt100[run in VT100 compatibility mode]'
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

	pager_views)
		_values -s , 'pager_views' auto long short
	;;

	algos)
		_values -s , 'algos' 1 2
	;;

	colorschemes)
		local -a color_schemes
		color_schemes=( $(basename -a $HOME/.config/clifm/colors/* | cut -d. -f1) )
		_multi_parts / color_schemes
	;;

	methods)
		_values -s , 'methods' none name size atime btime mtime version extension inode owner group blocks links type
	;;

	styles)
		_values -s , 'styles' default relative iso long-iso full-iso
	;;

	pstyles)
		_values -s , 'pstyles' default iso long-iso full-iso full-iso-nano
	;;

	workspaces)
		_values -s , 'workspaces' 1 2 3 4 5 6 7 8
	;;
esac
