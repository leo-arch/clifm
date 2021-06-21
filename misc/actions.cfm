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

i=img_viewer.sh
kbgen=kbgen
vid=vid_viewer.sh
ptot=pdf_viewer.sh
music=music_player.sh
update=update.sh
wall=wallpaper_setter.sh
dragon=dragondrop.sh
bn=batch_create.sh
+=finder.sh
++=jumper.sh
-=fzfnav.sh
*=fzfsel.sh
**=fzfdesel.sh
h=fzfhist.sh
//=rgfind.sh
ih=ihelp.sh
