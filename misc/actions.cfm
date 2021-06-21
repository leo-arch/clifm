######################
# CliFM actions file #
######################

# Define here your custom actions. Actions are custom command names
# binded to a shell script located in $XDG_CONFIG_HOME/clifm/PROFILE/scripts.
# Actions can be executed directly from CliFM command line, as if they
# were any other command, and the associated script will be executed
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
