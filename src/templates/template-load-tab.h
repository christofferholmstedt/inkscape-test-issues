#ifndef INKSCAPE_SEEN_UI_DIALOG_TEMPLATE_LOAD_TAB_H
#define INKSCAPE_SEEN_UI_DIALOG_TEMPLATE_LOAD_TAB_H

#include <gtkmm/box.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>

namespace Inkscape {
namespace UI {
    
    
class TemplateLoadTab : public Gtk::Frame
{

public:
    TemplateLoadTab();
    virtual ~TemplateLoadTab();
    virtual void createTemplate();

protected:
    class StringModelColumns : public Gtk::TreeModelColumnRecord
    {
        public:
        StringModelColumns()
        {
            add(textValue);
        }
        
        Gtk::TreeModelColumn<Glib::ustring> textValue;
    };
    
    Glib::ustring _current_keyword;
    Glib::ustring _current_template;
    
    virtual void _displayTemplateInfo();
    virtual void _initKeywordsList();
    virtual void _refreshTemplatesList();
    
    void _initLists();
    void _keywordSelected();    

    Gtk::HBox _main_box;
    Gtk::VBox _templates_column;
    Gtk::VBox _template_info_column;
    
    Gtk::ComboBoxText _keywords_combo;
    
    Gtk::TreeView _templates_view;
    Glib::RefPtr<Gtk::ListStore> _templates_ref;
    StringModelColumns _templates_columns;

};

}
}

#endif
