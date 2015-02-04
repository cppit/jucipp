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

        std::string ui_string() {
            return ui_string_;
        };
    private:
        std::string ui_string_;
    };

};

#endif //MODEL_H