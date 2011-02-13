# git-gui transport (fetch/push) support
# Copyright (C) 2006, 2007 Shawn Pearce

proc fetch_from {remote {close_after {}}} {
	global fetch_from_finished

	set fetch_from_finished 0
	set w [console::new \
		[mc "fetch %s" $remote] \
		[mc "Fetching new changes from %s" $remote]]
	set cmds [list]
	lappend cmds [list exec git fetch $remote]
	if {[is_config_true gui.pruneduringfetch]} {
		lappend cmds [list exec git remote prune $remote]
	}
	lappend cmds [list set fetch_from_finished 1]
	set ok [console::chain $w $cmds]
	load_all_remotes

	if {$ok} {
		if {$close_after ne {}} {
			console::close_window $w
		}
		return 1
	}
	return 0
}

proc prune_from {remote} {
	set w [console::new \
		[mc "remote prune %s" $remote] \
		[mc "Pruning tracking branches deleted from %s" $remote]]
	console::exec $w [list git remote prune $remote]
}

proc fetch_from_all {} {
	set w [console::new \
		[mc "fetch all remotes"] \
		[mc "Fetching new changes from all remotes"]]

	set cmd [list git fetch --all]
	if {[is_config_true gui.pruneduringfetch]} {
		lappend cmd --prune
	}

	console::exec $w $cmd
	load_all_remotes
}

proc prune_from_all {} {
	global all_remotes

	set w [console::new \
		[mc "remote prune all remotes"] \
		[mc "Pruning tracking branches deleted from all remotes"]]

	set cmd [list git remote prune]

	foreach r $all_remotes {
		lappend cmd $r
	}

	console::exec $w $cmd
}

proc push_to {remote} {
	set w [console::new \
		[mc "push %s" $remote] \
		[mc "Pushing changes to %s" $remote]]
	set cmd [list git push]
	lappend cmd -v
	lappend cmd $remote
	console::exec $w $cmd
	load_all_remotes
}

proc compose_email {to subject {body {}}} {
	set mail_link mailto:
	append mail_link $to "?subject=" $subject
	if {$body ne {}} {
		append mail_link "&body=" $body
	}
	start_browser $mail_link
}

proc after_push_anywhere_action {cons ok} {
	global push_email delete_after_push r_url branches

	console::done $cons $ok

	if {$ok} {
		if {$push_email} {
			set rconfig_var "remote.$r_url.url"
			set remote_url [exec git config $rconfig_var]
			set remote_short_name [lindex [split $remote_url / ] end ]
			set remote_short_name [lindex [split $remote_short_name . ] 0 ]
			# TODO: create a configuration variable for the
			# subject
			set email_subject "Review%20request:%20$remote_short_name:$branches"
			set email_body "url:review://$branches"
			compose_email {} $email_subject $email_body
		}
		if {$delete_after_push} {
			remote_branch_delete::dialog $r_url
		}
	}
}

proc do_push_on_change_branch {w} {
	global upstream_branch

	if {[is_config_true gui.emailafterpush]} {
		$w.options.email select
	}
	if {[is_config_true gui.deleteafterpush]} {
		$w.options.delete_after_push deselect
	}

	set cnt 0
	set b {}
	foreach i [$w.source.l curselection] {
		set b [$w.source.l get $i]
		incr cnt
	}

	if {$cnt != 1} {
		$w.options.email deselect
		$w.options.email configure -state disabled
		$w.options.delete_after_push deselect
		return
	}

	$w.options.email configure -state normal

	if { $b eq $upstream_branch } {
		if {[is_config_true gui.emailafterpush]} {
			$w.options.email deselect
		}
		if {[is_config_true gui.deleteafterpush]} {
			$w.options.delete_after_push select
		}
	}
}

proc start_push_anywhere_action {w} {
	global push_urltype push_remote push_url push_thin push_tags
	global push_force
	global repo_config
	global r_url branches

	set is_mirror 0
	set r_url {}
	switch -- $push_urltype {
	remote {
		set r_url $push_remote
		catch {set is_mirror $repo_config(remote.$push_remote.mirror)}
	}
	url {set r_url $push_url}
	}
	if {$r_url eq {}} return

	set cmd [list git push]
	lappend cmd -v
	if {$push_thin} {
		lappend cmd --thin
	}
	if {$push_force} {
		lappend cmd --force
	}
	if {$push_tags} {
		lappend cmd --tags
	}
	lappend cmd $r_url
	if {$is_mirror} {
		set cons [console::new \
			[mc "push %s" $r_url] \
			[mc "Mirroring to %s" $r_url]]
	} else {
		set cnt 0
		set branches {}
		foreach i [$w.source.l curselection] {
			set b [$w.source.l get $i]
			lappend cmd "refs/heads/$b:refs/heads/$b"
			lappend branches $b
			incr cnt
		}
		if {$cnt == 0} {
			return
		} elseif {$cnt == 1} {
			set unit branch
		} else {
			set unit branches
		}

		set cons [console::new \
			[mc "push %s" $r_url] \
			[mc "Pushing %s %s to %s" $cnt $unit $r_url]]
	}
	console::exec $cons $cmd "after_push_anywhere_action $cons"
	destroy $w
}

trace add variable push_remote write \
	[list radio_selector push_urltype remote]

proc do_push_anywhere {} {
	global all_remotes current_branch
	global push_urltype push_remote push_url push_thin push_tags
	global push_force use_ttk NS push_email upstream_branch
	global repo_config

	set w .push_setup
	toplevel $w
	catch {wm attributes $w -type dialog}
	wm withdraw $w
	wm geometry $w "+[winfo rootx .]+[winfo rooty .]"
	pave_toplevel $w

	${NS}::label $w.header -text [mc "Push Branches"] \
		-font font_uibold -anchor center
	pack $w.header -side top -fill x

	${NS}::frame $w.buttons
	${NS}::button $w.buttons.create -text [mc Push] \
		-default active \
		-command [list start_push_anywhere_action $w]
	pack $w.buttons.create -side right
	${NS}::button $w.buttons.cancel -text [mc "Cancel"] \
		-default normal \
		-command [list destroy $w]
	pack $w.buttons.cancel -side right -padx 5
	pack $w.buttons -side bottom -fill x -pady 10 -padx 10

	${NS}::labelframe $w.source -text [mc "Source Branches"]
	slistbox $w.source.l \
		-height 10 \
		-width 70 \
		-selectmode extended
	if {[is_config_true gui.emailafterpush] \
	    || [is_config_true gui.deleteafterpush]} {
		bind $w.source.l <ButtonRelease-1> [list do_push_on_change_branch $w]
	}
	foreach h [load_all_heads] {
		$w.source.l insert end $h
		if {$h eq $current_branch} {
			$w.source.l select set end
			$w.source.l yview end
		}
	}
	pack $w.source.l -side left -fill both -expand 1
	pack $w.source -fill both -expand 1 -pady 5 -padx 5

	${NS}::labelframe $w.dest -text [mc "Destination Repository"]
	if {$all_remotes ne {}} {
		${NS}::radiobutton $w.dest.remote_r \
			-text [mc "Remote:"] \
			-value remote \
			-variable push_urltype
		if {$use_ttk} {
			ttk::combobox $w.dest.remote_m -state readonly \
				-exportselection false \
				-textvariable push_remote \
				-values $all_remotes
		} else {
			eval tk_optionMenu $w.dest.remote_m push_remote $all_remotes
		}
		grid $w.dest.remote_r $w.dest.remote_m -sticky w
		if {[lsearch -sorted -exact $all_remotes origin] != -1} {
			set push_remote origin
		} else {
			set push_remote [lindex $all_remotes 0]
		}
		set push_urltype remote
	} else {
		set push_urltype url
	}
	${NS}::radiobutton $w.dest.url_r \
		-text [mc "Arbitrary Location:"] \
		-value url \
		-variable push_urltype
	${NS}::entry $w.dest.url_t \
		-width 50 \
		-textvariable push_url \
		-validate key \
		-validatecommand {
			if {%d == 1 && [regexp {\s} %S]} {return 0}
			if {%d == 1 && [string length %S] > 0} {
				set push_urltype url
			}
			return 1
		}
	grid $w.dest.url_r $w.dest.url_t -sticky we -padx {0 5}
	grid columnconfigure $w.dest 1 -weight 1
	pack $w.dest -anchor nw -fill x -pady 5 -padx 5

	${NS}::labelframe $w.options -text [mc "Transfer Options"]
	${NS}::checkbutton $w.options.force \
		-text [mc "Force overwrite existing branch (may discard changes)"] \
		-variable push_force
	grid $w.options.force -columnspan 2 -sticky w
	${NS}::checkbutton $w.options.thin \
		-text [mc "Use thin pack (for slow network connections)"] \
		-variable push_thin
	grid $w.options.thin -columnspan 2 -sticky w
	${NS}::checkbutton $w.options.tags \
		-text [mc "Include tags"] \
		-variable push_tags
	grid $w.options.tags -columnspan 2 -sticky w
	checkbutton $w.options.email \
		-text [mc "Compose email with review request"] \
		-variable push_email
	grid $w.options.email -columnspan 2 -sticky w
	$w.options.email deselect
	checkbutton $w.options.delete_after_push \
		-text [mc "Launch delete dialog after push"] \
		-variable delete_after_push
	grid $w.options.delete_after_push -columnspan 2 -sticky w
	$w.options.delete_after_push deselect
	grid columnconfigure $w.options 1 -weight 1
	pack $w.options -anchor nw -fill x -pady 5 -padx 5

	set push_url {}
	set push_force 0
	set push_thin 0
	set push_tags 0
	set push_email 0
	set delete_after_push 0

	set upstream_branch $repo_config(gui.upstreambranch)

	if {[is_config_true gui.emailafterpush] \
	    || [is_config_true gui.deleteafterpush]} {
		do_push_on_change_branch $w
	}

	bind $w <Visibility> "grab $w; focus $w.buttons.create"
	bind $w <Key-Escape> "destroy $w"
	bind $w <Key-Return> [list start_push_anywhere_action $w]
	wm title $w [append "[appname] ([reponame]): " [mc "Push"]]
	wm deiconify $w
	tkwait window $w
}

proc do_import_patches {} {
	global _gitworktree

	set exe [list [_which git]]
	if {$exe eq {}} {
		error_popup [mc "Couldn't find git gui in PATH"]
		return
	}

	lock_index read

	set types {
		{{Patch Files}	{.patch}}
		{{All Files}	*	}
	}

	set p [tk_getOpenFile \
		-initialdir $_gitworktree \
		-parent . \
		-title [mc "Choose patches for import ..."] \
		-multiple true \
		-filetypes $types]
	if {$p eq {}} {
		unlock_index
		return
	}

	set p [file normalize $p]
	set p [lsort $p]

	set cmd [lappend exe am -3 --]
	append cmd { } $p

	if {[catch {eval exec $cmd} {err}]} {
		if {[ask_popup [mc "Applying patch series failed: %s\n\n\
Do you want to abort?" $err]] eq {yes}} {
			set cmd [list $exe am --abort]
			catch {eval exec $cmd}
		}
	}

	unlock_index
	ui_do_rescan
}

proc do_abort_import_patches {} {
	set exe [list [_which git]]
	if {$exe eq {}} {
		error_popup [mc "Couldn't find git gui in PATH"]
		return
	}

	lock_index read

	if {[ask_popup [mc "Do you want to abort applying patches?"]] \
		eq {yes}} {
		set cmd [list $exe am --abort]
		catch {eval exec $cmd}
	}
	unlock_index
	ui_do_rescan
}
