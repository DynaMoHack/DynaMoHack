# src/CMakeLists.txt : build libdynamohack

set (LIBDYNAMOHACK_SRC
    allmain.c  apply.c    artifact.c attrib.c   ball.c    bones.c
    botl.c     cmd.c      dbridge.c  decl.c     detect.c  dig.c      display.c
    dlb.c      do.c       dog.c      dogmove.c  dokick.c  do_name.c  dothrow.c
    do_wear.c  drawing.c  dump.c     dungeon.c  eat.c     end.c      engrave.c  exper.c
    explode.c  extralev.c files.c    fountain.c hack.c    hacklib.c  history.c invent.c
    light.c    lock.c     log.c      logreplay.c makemon.c mcastu.c  memfile.c mhitm.c    mhitu.c
    minion.c   mklev.c    mkmap.c    mkmaze.c   mkobj.c   mkroom.c   mon.c
    mondata.c  monmove.c  monst.c    mplayer.c  mthrowu.c mtrand.c   muse.c     music.c
    objects.c  objnam.c   o_init.c   options.c  pager.c   pickup.c   pline.c
    polyself.c potion.c   pray.c     priest.c   quest.c   questpgr.c read.c
    rect.c     region.c   restore.c  role.c     rumors.c  save.c
    shk.c      shknam.c   sit.c      sounds.c   spell.c   sp_lev.c   symclass.c
    steal.c    steed.c    teleport.c timeout.c  topten.c  track.c    trap.c    tutorial.c
    uhitm.c    u_init.c   vault.c    version.c  vision.c  weapon.c   were.c
    wield.c    windows.c  wizard.c   worm.c     worn.c    write.c    xmalloc.c zap.c
    )
set (LIBDYNAMOHACK_GENERATED_SRC
    ${LNH_SRC_GEN}/monstr.c
    )

set_source_files_properties(${LIBDYNAMOHACK_GENERATED_SRC} GENERATED)

set(MAKEDEFS_BIN $<TARGET_FILE:makedefs>)


add_custom_command (OUTPUT ${LNH_SRC_GEN}/monstr.c
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -m ${LNH_SRC_GEN}/monstr.c
                    DEPENDS makedefs)

include_directories (
    ${DynaMoHack_SOURCE_DIR}/libdynamohack/include
    ${DynaMoHack_SOURCE_DIR}/include
    ${DynaMoHack_BINARY_DIR}/libdynamohack/include )

add_library(libdynamohack ${LIB_TYPE} ${LIBDYNAMOHACK_SRC} ${LIBDYNAMOHACK_GENERATED_SRC})
set_target_properties(libdynamohack PROPERTIES OUTPUT_NAME dynamohack )
target_link_libraries(libdynamohack z) 

add_dependencies (libdynamohack makedefs_headers)

if (NOT ALL_STATIC)
    install(TARGETS libdynamohack
            DESTINATION ${LIBDIR})
endif ()
