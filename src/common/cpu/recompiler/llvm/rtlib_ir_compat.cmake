





if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "INPUT and OUTPUT are required")
endif()
file(READ "${INPUT}" ir)

string(REGEX REPLACE "captures\\([^)]*\\)[ \t]*" "" ir "${ir}")

string(REGEX REPLACE "samesign[ \t]+" "" ir "${ir}")

string(REGEX REPLACE "nocreateundeforpoison[ \t]+" "" ir "${ir}")


string(REGEX REPLACE "call void @llvm\\.lifetime\\.[^\n]*\n" "" ir "${ir}")
string(REGEX REPLACE "declare void @llvm\\.lifetime\\.[^\n]*\n" "" ir "${ir}")
file(WRITE "${OUTPUT}" "${ir}")
