target_sources(tsolvers
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/LRASolver.h"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LAVar.h"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Delta.h"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LABounds.h"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LARefs.h"
    #PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LRAModel.h"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Tableau.h"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Polynomial.h"

    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LRASolver.C"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LAVar.C"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Delta.C"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LABounds.C"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/LRAModel.cpp"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Tableau.cpp"
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Polynomial.cpp"
    )

if(PRODUCE_PROOF)
    target_sources(tsolvers
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/LRA_Interpolator.h"
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/LRA_Interpolator.cpp"
        )
endif()

