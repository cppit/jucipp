/*juCi++ model header file*/
#ifndef MODEL_H
#define MODEL_H

#include "gtkmm.h"

/* -------------------------- HOW TO -------------------------- */
/* Model classes under Model if possible                        */
/* ------------------ Remove these comments ------------------- */

class Model {
public:
    class Menu {
    public:
        Menu();

        virtual~Menu();

        std::string get_ui_string() {
            return ui_string;
        };
    private:
        std::string ui_string;
    };

};

#endif //MODEL_H