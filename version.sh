#!/bin/sh

if [ `which svnversion` ]; then
	REPOREV=`svnversion -n .`
elif [ -f ".svn/entries" ]; then
	while read line; do
		if [ "$lastline" = "dir" ]; then
			REPOREV="$line"U
			break 1
		fi
		lastline="$line"
	done < ".svn/entries"
	if [ "$REPOREV" = "" ]; then
		REPOREV="export"
	fi
else
	REPOREV='export'
fi
if [ -f ddns_version.h ]; then
	REV=`cat ddns_version.h | cut -d' ' -f 3`
	if [ $REV != "\"$REPOREV\"" ]; then
		echo "Updating version to $REPOREV..."
		echo "#define REPOSITORY_REVISION \"$REPOREV\"" > ddns_version.h
	fi
else
	echo "Updating version to $REPOREV..."
	echo "#define REPOSITORY_REVISION \"$REPOREV\"" > ddns_version.h
fi
