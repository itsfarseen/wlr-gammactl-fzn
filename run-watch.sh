#!/bin/sh

# Open run-watch.sh in a terminal in the background to have edits to run.sh
# reflected live on your screen.

echo ./run.sh | entr -rc ./run.sh
