######################
# CliFM actions file #
######################

# Define here your custom actions. Actions are custom command names
# bound to a executable file located either in DATADIR/clifm/plugins
# (usually /usr/share/clifm/plugins) or in $XDG_CONFIG_HOME/clifm/plugins.
# Actions can be executed directly from CliFM command line, as if they
# were any other command, and the associated file will be executed
# instead. All parameters passed to the action command will be passed
# to the corresponding plugin as well.

+=finder.sh
++=jumper.sh
-=fzfnav.sh
*=fzfsel.sh
**=fzfdesel.sh
//=rgfind.sh
_=fzcd.sh
bn=batch_create.sh
cr=cprm.sh
da=disk_analyzer.sh
dh=fzfdirhist.sh
dr=dragondrop.sh
fdups=fdups.sh
gg=pager.sh
h=fzfhist.sh
i=img_viewer.sh
ih=ihelp.sh
kbgen=kbgen
music=music_player.sh
ptot=pdf_viewer.sh
rrm=recur_rm.sh
update=update.sh
vid=vid_viewer.sh
wall=wallpaper_setter.sh
