#!/bin/bash


# ----------------------------------------------------------------------------
# First parameter to the script is the name of the module to install.  
# ----------------------------------------------------------------------------


MODFILE=${1}
shift

if [ "" == "$MODFILE" ]; then
	echo Module name not defined.
	exit
fi

if [ -x /sbin/lsmod ]; then
	LSMOD=/sbin/lsmod
else
	LSMOD=lsmod
fi

# ----------------------------------------------------------------------------
# By default, the install script operates in a `replace' mode
# i.e if a module is already installed, it will be removed and 
# then re-installed.
# Change the value of REPLACE to "false" if you want the install
# to fail if the module is already loaded.
# ----------------------------------------------------------------------------
REPLACE="true"

MODFILE2=`echo $MODFILE | sed -e 's/\.ko$/\.o/'`
MODNAME=`basename ${MODFILE2} ".o"`
MODNAME2=`echo $MODNAME | sed -e 's/-/_/g'`

# ----------------------------------------------------------------------------
# Check if a module is loaded.
# exit status: 0 if the module is loaded, non-zero otherwise.

isModuleLoaded() 
{
	$LSMOD | grep -q -e "^${MODNAME2}[[:space:]]" 
}

# ----------------------------------------------------------------------------
# Remove module if necessary.

if isModuleLoaded  ; then
    if [[  ${REPLACE} != "true" ]] ; then
	echo "Module ${MODNAME} is already loaded: Skipping."
	exit
    else
    	echo "Removing " ${MODNAME}
    	/sbin/rmmod ${MODNAME}
    	sleep 1
    fi
fi

# ----------------------------------------------------------------------------
# Install the kernel module; list modules to show it has been installed
# ----------------------------------------------------------------------------
/sbin/insmod $MODFILE  $*
$LSMOD

if isModuleLoaded  ; then
    	echo ${MODNAME} " loaded successfully."
else
    	echo ${MODNAME} " loading failed."
fi
