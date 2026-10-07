#!/usr/bin/env janus-run
# DESIGN.md s4 worked example.
#
# Run with the engine:   ./nested-async.sh         (shebang -> janus-run)
# Debug sequentially:    change shebang to #!/usr/bin/env bash

#@ group async: outer
    #@ step: step 1
    echo "step 1 start"; sleep 1; echo "step 1 done"
    #@ end
    #@ step: step 2
    echo "step 2 start"; sleep 1; echo "step 2 done"
    #@ end
    #@ group async: nested
        #@ step: step 3
        echo "step 3 start"; sleep 1; echo "step 3 done"
        #@ end
        #@ step: step 4
        echo "step 4 start"; sleep 1; echo "step 4 done"
        #@ end
    #@ end
    #@ step: step 5
    echo "step 5 start"; sleep 1; echo "step 5 done"
    #@ end
#@ end
