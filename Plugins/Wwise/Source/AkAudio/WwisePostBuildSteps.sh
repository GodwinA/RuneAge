#!/bin/bash

editorPath=$1
shift

if [ "$editorPath" = "" ]; then exit 0
fi

if [ $1 = "" ]; then exit 0
fi

if [ $1 = "\$(ProjectFile)" ]; then exit 0
fi

if [ "$SKIP_PLUGIN_ACTIVATION" = "true" ]; then exit 0
fi

if [ "$2" = "Editor" ]; then 
echo "Skipping plugin activation for editor target"
exit 0 
fi

if [ ! -e "$editorPath" ]; then
    echo "WwisePostBuildSteps ERROR: Can't find $editorPath"
    exit 1
fi

"$editorPath" "$@"
exit $?
