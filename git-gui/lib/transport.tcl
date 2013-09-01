# git-gui transport (fetch/push) support
# Copyright (C) 2006, 2007 Shawn Pearce

proc fetch_from {remote} {
	set w [console::new \
		[mc "fetch %s" $remote] \
		[mc "Fetching new changes from %s" $remote]]
	set cmds [list]
	lappend cmds [list exec git fetch $remote]
	if {[is_config_true gui.pruneduringfetch]} {
		lappend cmds [list exec git remote prune $remote]
	}
	console::chain $w $cmds
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
}

proc get_remote_rep {} {
	global push_urltype push_remote push_url 
	set rep {}
	switch -- $push_urltype {
	remote { set rep $push_remote }
	url    { set rep $push_url }
	}
	return $rep
}

proc get_remote_branch {} {
	global push_branchtype push_branch push_new 
	set branch {}
	switch -- $push_branchtype {
	existing { set branch $push_branch }
	create   { set branch $push_new }
	}
   return $branch
}

proc get_remote_ref_spec {} {
	global gerrit_review
	set push_branch [get_remote_branch]
	if {$gerrit_review} {
		return "refs/for/$push_branch"
	} else {
		return "refs/heads/$push_branch"
	}
}

proc start_push_anywhere_action {w} {
	global push_thin push_tags push_force
	global repo_config current_branch is_detached

	set is_mirror 0
	set r_url [get_remote_rep]
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
	
	catch {set is_mirror $repo_config(remote.$r_url.mirror)}
	if {$is_mirror} {
		set cons [console::new \
			[mc "push %s" $r_url] \
			[mc "Mirroring to %s" $r_url]]
	} else {
		if {$is_detached} {
			set src HEAD
		} else {
			set src $current_branch
		}
		set dest [get_remote_ref_spec]
		
		lappend cmd "$src:$dest"

		set cons [console::new \
			[mc "push %s" $r_url] \
			[mc "Pushing %s to %s" $src $dest]]
	}
	console::exec $cons $cmd
	destroy $w
}

trace add variable push_remote write \
	[list radio_selector push_urltype remote]

proc update_branchtype {br} {
	global current_branch push_branch push_branchtype
	if {$br eq {}} {
		set push_branchtype create
		set push_branch {}
	} else {
		set push_branchtype existing
		if {[lsearch -sorted -exact $br $current_branch] != -1} {
			set push_branch $current_branch
		} elseif {[lsearch -sorted -exact $br master] != -1} {
			set push_branch master
		} else {
			set push_branch [lindex $br 0]
		}
	}
}
	
proc all_branches_combined {} {
	set branches [list]
	foreach spec [all_tracking_branches] {
		set refn [lindex $spec 2]
		regsub ^refs/heads/ $refn {} name
		if { $name ne {HEAD} && [lsearch $branches $name] eq -1} {
			lappend branches $name
		}
	}
	update_branchtype  $branches
	return $branches
}

proc update_branches {} {
	global push_remote branch_combo
	set branches [list]
	foreach spec [all_tracking_branches] {
		if {[lindex $spec 1] eq $push_remote} {
			set refn [lindex $spec 0]
			regsub ^refs/(heads|remotes)/$push_remote/ $refn {} name
			if {$name ne {HEAD}} {
				lappend branches $name
			}
		}
	}
	update_branchtype  $branches
	$branch_combo configure -values $branches
	return $branches
}

proc do_push_anywhere {} {
	global all_remotes use_ttk branch_combo
	global push_urltype push_remote push_url
	global push_branchtype push_branch push_new 
	global push_thin push_tags push_force NS gerrit_review

	set w .push_setup
	toplevel $w
	catch {wm attributes $w -type dialog}
	wm withdraw $w
	wm geometry $w "+[winfo rootx .]+[winfo rooty .]"
	pave_toplevel $w

	${NS}::label $w.header -text [mc "Push current HEAD"] \
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

	${NS}::labelframe $w.destbr -text [mc "Destination Branches"]
	set all_branches [all_branches_combined]
	if {$all_branches ne {}} {
		${NS}::radiobutton $w.destbr.remote_b \
			-text [mc "Known Branch:        "] \
			-value existing \
			-variable push_branchtype
		if {$use_ttk} {
			ttk::combobox $w.destbr.remote_n -state readonly \
				-exportselection false \
				-textvariable push_branch
			set branch_combo $w.destbr.remote_n
			update_branches
		} else {
			eval tk_optionMenu $w.destbr.remote_n push_branch $all_branches
		}
		grid $w.destbr.remote_b $w.destbr.remote_n -sticky w
	}
		
	${NS}::radiobutton $w.destbr.branch_r \
		-text [mc "Arbitrary Branch:"] \
		-value create \
		-variable push_branchtype
	${NS}::entry $w.destbr.branch_t \
		-width 50 \
		-textvariable push_new \
		-validate key \
		-validatecommand {
			if {%d == 1 && [regexp {\s} %S]} {return 0}
			if {%d == 1 && [string length %S] > 0} {
				set push_branchtype create
			}
			return 1
		}
	grid $w.destbr.branch_r $w.destbr.branch_t -sticky we -padx {0 5}
	${NS}::checkbutton $w.destbr.gerrit \
		-text [mc "Push for Gerrit review (refs/for/...)"] \
		-variable gerrit_review
	grid $w.destbr.gerrit -columnspan 2 -sticky w

	grid columnconfigure $w.destbr 1 -weight 1
	pack $w.destbr -anchor nw -fill x -pady 5 -padx 5

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
	grid columnconfigure $w.options 1 -weight 1
	pack $w.options -anchor nw -fill x -pady 5 -padx 5

	set push_url {}
	set push_force 0
	set push_thin 0
	set push_tags 0
	set gerrit_review 0

	bind $w <Visibility> "grab $w; focus $w.buttons.create"
	bind $w <Key-Escape> "destroy $w"
	bind $w <Key-Return> [list start_push_anywhere_action $w]
	if {$all_remotes ne {}} {
		bind $w.dest.remote_m <<ComboboxSelected>> { update_branches }
	}
	wm title $w [append "[appname] ([reponame]): " [mc "Push"]]
	wm deiconify $w
	tkwait window $w
}
