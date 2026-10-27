// ----------------------------------------------------------------------------
// Database.cpp
// ----------------------------------------------------------------------------
#include "Database.h"
#include <algorithm>
#include <cctype>

bool database_enabled = true;
std::string database_filename;

static std::string database_GetValue(std::string entry) {
  int index = entry.rfind('=');
  return entry.substr(index + 1);
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
void database_Initialize( ) {
  database_filename = common_defaultPath + "ProSystem.dat";
}

// ----------------------------------------------------------------------------
// Load
// ----------------------------------------------------------------------------
bool database_Load(std::string digest) {   
    if (!database_enabled) return false;  

    FILE* file = fopen(database_filename.c_str(), "r");   
    if (file == NULL) {   
        return false;   
    }   

    std::string targetDigest = digest;   
    std::transform(targetDigest.begin(), targetDigest.end(), targetDigest.begin(), ::tolower);   

    char buffer[256];   
    while (fgets(buffer, sizeof(buffer), file) != NULL) {   
        std::string line = buffer;   
        if (line.length() >= 33 && line[0] == '[') {   
            std::string lineDigest = line.substr(1, 32);   
            std::transform(lineDigest.begin(), lineDigest.end(), lineDigest.begin(), ::tolower);   

            if (lineDigest == targetDigest) {   
                while (fgets(buffer, sizeof(buffer), file) != NULL) {   
                    std::string prop = common_Trim(common_Remove(common_Remove(buffer, '\n'), '\r'));   
                    if (prop.empty() || prop[0] == '[') {   
                        break; // End of this ROM's section  
                    }   
                    size_t eqPos = prop.find('=');   
                    if (eqPos != std::string::npos) {   
                        std::string key = common_Trim(prop.substr(0, eqPos));   
                        std::string val = common_Trim(prop.substr(eqPos + 1));   
                        if (key == "title") cartridge_title = val;   
                        else if (key == "type") cartridge_type = common_ParseByte(val);   
                        else if (key == "pokey") cartridge_pokey = common_ParseBool(val);   
                        else if (key == "controller1") cartridge_controller[0] = common_ParseByte(val);   
                        else if (key == "controller2") cartridge_controller[1] = common_ParseByte(val);   
                        else if (key == "region") cartridge_region = common_ParseByte(val);   
                        else if (key == "flags") cartridge_flags = common_ParseUint(val);  
                        else if (key == "crossx") cartridge_crossx = common_ParseInt(val);  
                        else if (key == "crossy") cartridge_crossy = common_ParseInt(val);  
                        else if (key == "hsc") cartridge_hsc = common_ParseBool(val);  
                        else if (key == "xm") cartridge_xm = common_ParseBool(val);  
                        else if (key == "pokey450") cartridge_pokey450 = common_ParseBool(val);  
                        else if (key == "hblank") cartridge_hblank = common_ParseByte(val);  
                        else if (key == "dualanalog") cartridge_dualanalog = common_ParseBool(val);  
                        else if (key == "leftswitch") cartridge_leftswitch = common_ParseBool(val);  
                        else if (key == "rightswitch") cartridge_rightswitch = common_ParseBool(val);  
                        else if (key == "swapbuttons") cartridge_swapbuttons = common_ParseBool(val);  
                    }   
                }   
                fclose(file);   
                return true;   
            }   
        }   
    }   
    fclose(file);   
    return false;   
}
