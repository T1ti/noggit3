// This file is part of Noggit3, licensed under GNU General Public License (version 3).
#include <cmath>

#include <noggit/camera.hpp>
#include <noggit/Log.h>
#include <noggit/scripting/scripting_tool.hpp>
#include <noggit/tool_enums.hpp>
#include <noggit/World.h>
#include <noggit/scripting/script_context.hpp>
#include <noggit/scripting/script_exception.hpp>
#include <noggit/scripting/script_profiles.hpp>
#include <noggit/scripting/script_settings.hpp>
#include <noggit/MapView.h>

#include <QtWidgets/QFormLayout>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QCheckBox>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#define CUR_PROFILE_PATH "__cur_profile"

namespace noggit
{
  namespace scripting
  {
    void scripting_tool::doReload()
    {
      get_settings()->clear();
      clearLog();
      try
      {
        get_context()->reset(this);
      }
      catch (std::exception const& e)
      {
        addLog("[error]: " + std::string(e.what()));
        resetLogScroll();
        return;
      }
      int selection = get_context()->get_selection();

      _selection->clear();

      for(auto& script : get_context()->get_scripts())
      {
        _selection->addItem(script.get_name().c_str());
      }

      if (selection != -1)
      {
        _selection->setCurrentIndex(selection);
        change_script(selection);
      }
    }

    void scripting_tool::change_script(int selection)
    {
      std::lock_guard<std::mutex> const lock (_script_change_mutex);

      clearDescription();
      get_settings()->clear();

      auto sn = _script_context->get_scripts()[selection].get_name();

      get_profiles()->clear();

      auto json = get_settings()->get_raw_json();

      if (json->contains(sn))
      {
        std::vector<std::string> items;
        for (auto& v : (*json)[sn].items())
        {
          if (v.key() != CUR_PROFILE_PATH)
          {
            items.push_back(v.key());
          }
        }

        std::sort(items.begin(), items.end(), [](auto a, auto b) {
          if (a == "Default")
            return true;
          if (b == "Default")
            return false;
          return a < b;
        });

        for (auto& item : items)
        {
          get_profiles()->add_profile(item);
        }
      }

      if (get_profiles()->profile_count() == 0)
      {
        get_profiles()->add_profile("Default");
      }

      int next_profile = 0;
      auto cur_script = get_context()->get_scripts()[selection].get_name();
      if (json->contains(cur_script))
      {
        if ((*json)[cur_script].contains(CUR_PROFILE_PATH))
        {
          auto str = (*json)[cur_script][CUR_PROFILE_PATH].get<std::string>();
          for (int i = 0; i < get_profiles()->profile_count(); ++i)
          {
            if (get_profiles()->get_profile(i) == str)
            {
              next_profile = i;
              break;
            }
          }
        }
      }

      get_profiles()->select_profile(next_profile);

      try {
        get_context()->select_script(selection);
      } catch(script_exception &const err)
      {
        addLog(err.what());
      }
      get_settings()->initialize();
    }

    scripting_tool::scripting_tool(QWidget* parent, MapView* view)
      : QWidget(parent)
      , _cur_profile ("Default")
      , _view(view)
      , _script_context(new script_context())
    {
      auto layout(new QVBoxLayout(this));
      _selection = new QComboBox();
      layout->addWidget(_selection);

      _reload_button = new QPushButton("Reload Scripts", this);
      layout->addWidget(_reload_button);
      connect(_reload_button, &QPushButton::released, this, [this]() {
        doReload();
      });

      _profiles = new script_profiles(this);
      layout->addWidget(_profiles);

      _settings = new script_settings(this);
      _settings->load_json();
      layout->addWidget(_settings);

      _description = new QLabel(this);
      layout->addWidget(_description);

      _log = new QPlainTextEdit(this);
      _log->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
      _log->setReadOnly(true);
      layout->addWidget(_log);

      connect(_selection, QOverload<int>::of(&QComboBox::activated), this, [this](auto index) {
        clearLog();
        change_script(index);
      });

      doReload();
    }

    scripting_tool::~scripting_tool()
    {
      get_settings()->save_json();
    }

    void scripting_tool::sendBrushEvent(math::vector_3d const& pos, float dt)
    {
      bool new_left = get_view()->leftMouse;
      bool new_right = get_view()->rightMouse;

      auto evt = script_brush_event(
          pos
        , get_settings()->brushRadius()
        , get_settings()->innerRadius()
        , dt
      );

      try
      {
        int sel = get_context()->get_selection();
        if(sel>=0)
        {
          auto brush = & get_context()->get_scripts()[sel];
          if(new_left)
          {
            if(!_last_left) brush->_left_click.call_if_not_null("(brush_event)",evt);
            else brush->_left_hold.call_if_not_null("(brush_event)",evt);
          }
          else
          {
            if(_last_left) brush->_left_release.call_if_not_null("(brush_event)",evt);
          }

          if(new_right)
          {
            if(!_last_right) brush->_right_click.call_if_not_null("(brush_event)",evt);
            else brush->_right_hold.call_if_not_null("(brush_event)",evt);
          }
          else
          {
            if(_last_right) brush->_right_release.call_if_not_null("(brush_event)",evt);
          }
        }
      }
      catch (std::exception const& e)
      {
        doReload();
        addLog(("[error]: " + std::string(e.what())));
        resetLogScroll();
      }
      _last_left = new_left;
      _last_right = new_right;
    }

    void scripting_tool::addDescription(std::string const& stext)
    {
      _description->setText(_description->text() + "\n" + QString::fromStdString (stext));
    }

    void scripting_tool::addLog(std::string const& text)
    {

      LogDebug << "[script window]: " << text << "\n";
      _log->appendPlainText (QString::fromStdString (text));
      _log->verticalScrollBar()->setValue(_log->verticalScrollBar()->maximum());
    }

    script_context* scripting_tool::get_context()
    {
      return _script_context;
    }

    MapView* scripting_tool::get_view()
    {
      return _view;
    }

    void scripting_tool::resetLogScroll()
    {
      _log->verticalScrollBar()->setValue(0);
    }

    void scripting_tool::clearLog()
    {
      _log->clear();
    }

    void scripting_tool::clearDescription()
    {
      _description->clear();
    }

    script_settings* scripting_tool::get_settings()
    {
      return _settings;
    }

    script_profiles* scripting_tool::get_profiles()
    {
      return _profiles;
    }
  } // namespace scripting
} // namespace noggit
