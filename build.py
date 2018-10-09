#!/bin/python
import os, sys, subprocess

if not os.path.isdir( os.path.expanduser("~/blender-fork") ):
	os.mkdir( os.path.expanduser("~/blender-fork") )

	subprocess.check_call(
		['git', 'clone', 'https://git.blender.org/blender.git'], 
		cwd=os.path.expanduser("~/blender-fork")
	)

	subprocess.check_call(
		['git', 'submodule', 'update', '--init', '--recursive'], 
		cwd=os.path.expanduser("~/blender-fork/blender")
	)

	subprocess.check_call(
		['git', 'submodule', 'foreach', 'git', 'checkout', 'master'], 
		cwd=os.path.expanduser("~/blender-fork/blender")
	)

	subprocess.check_call(
		['git', 'checkout', 'blender2.8'], 
		cwd=os.path.expanduser("~/blender-fork/blender")
	)
else:
	subprocess.check_call(
		['git', 'pull'], 
		cwd=os.path.expanduser("~/blender-fork/blender")
	)

subprocess.check_call( ['cp', '-Rv', 'blender/*', os.path.expanduser("~/blender-fork/blender/")] )
subprocess.check_call(
	['make'], 
	cwd=os.path.expanduser("~/blender-fork/blender")
)

##TODO run cmake to build the plugin, and then copy it to the blender-fork/blender/...
