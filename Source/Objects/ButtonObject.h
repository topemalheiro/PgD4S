/*
 // Copyright (c) 2021-2025 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */
#pragma once

class ButtonObject final : public ObjectBase {

    bool state = false;
    bool alreadyTriggered = false;

    Value primaryColour = SynchronousValue();
    Value secondaryColour = SynchronousValue();
    Value transparent = SynchronousValue();
    Value sizeProperty = SynchronousValue();
    Value readOnly = SynchronousValue();
    Value oval = SynchronousValue();
    Value ovalOutline = SynchronousValue();

    // PgD4S: a button with its own text (Max's live.text), stored as attributes ("#X attr"), as ELSE's
    // button has no arguments for them
    Value text = SynchronousValue();
    Value textOn = SynchronousValue();
    Value textColour = SynchronousValue();
    Value textOnColour = SynchronousValue();
    Value fontSize = SynchronousValue();
    Value textAlign = SynchronousValue();
    std::optional<Colour> setTextColour, setTextOnColour;

    NVGcolor fgCol;
    NVGcolor bgCol;

    enum Mode {
        Latch,
        Toggle,
        Bang,
        Click
    };

    Mode mode;

public:
    ButtonObject(pd::WeakReference obj, Object* parent)
        : ObjectBase(obj, parent)
    {
        objectParameters.addParamSize(&sizeProperty);
        objectParameters.addParamColourFG(&primaryColour);
        objectParameters.addParamColourBG(&secondaryColour);
        objectParameters.addParamBool("Transparent", cAppearance, &transparent, { "No", "Yes" });
        objectParameters.addParamBool("Oval", cAppearance, &oval, { "No", "Yes" });
        objectParameters.addParamBool("Oval outline", cAppearance, &ovalOutline, { "No", "Yes" }, 1);
        objectParameters.addParamBool("Read Only", cGeneral, &readOnly, { "No", "Yes" });

        objectParameters.addParamString("Text", cLabel, &text, "");
        objectParameters.addParamString("Text when on", cLabel, &textOn, "");
        objectParameters.addParamColour("Text colour", cLabel, &textColour, PlugDataColour::canvasTextColourId);
        objectParameters.addParamColour("Text colour when on", cLabel, &textOnColour, PlugDataColour::canvasTextColourId);
        objectParameters.addParamInt("Font size", cLabel, &fontSize, 10, true, 4);
        objectParameters.addParamCombo("Alignment", cLabel, &textAlign, { "Left", "Centred", "Right" }, 2);

        updateColours();
    }

    static inline StringArray const alignNames { "left", "center", "right" };

    static std::optional<Colour> parseColour(String const& hex)
    {
        if (hex.isEmpty())
            return std::nullopt;
        return Colour::fromString(hex.startsWith("#") ? "ff" + hex.substring(1) : hex);
    }

    static String toHex(Colour const colour)
    {
        return "#" + colour.toDisplayString(false).toLowerCase();
    }

    // PgD4S: set without the property listener, so reading never writes attributes back
    void readAttributes()
    {
        setParameterExcludingListener(text, getAttribute("text"));
        setParameterExcludingListener(textOn, getAttribute("texton"));
        setTextColour = parseColour(getAttribute("textcolor"));
        setTextOnColour = parseColour(getAttribute("textoncolor"));
        setParameterExcludingListener(textColour, getShownTextColour(false).toString());
        setParameterExcludingListener(textOnColour, getShownTextColour(true).toString());
        setParameterExcludingListener(fontSize, std::max(getAttribute("fontsize", "10").getIntValue(), 4));
        setParameterExcludingListener(textAlign, std::max(1, alignNames.indexOf(getAttribute("align", "center")) + 1));
        repaint();
    }

    bool hasText() const
    {
        return text.toString().isNotEmpty() || textOn.toString().isNotEmpty();
    }

    // the colour set for each state; unset, a colour that reads on the button (the "on" text takes the
    // "off" colour when only that one is set)
    Colour getShownTextColour(bool const on) const
    {
        if (on && setTextOnColour)
            return *setTextOnColour;
        if (setTextColour)
            return *setTextColour;
        if (getValue<bool>(transparent))
            return PlugDataColours::canvasTextColour;
        return Colour::fromString((on ? primaryColour : secondaryColour).toString()).contrasting();
    }

    void renderText(NVGcontext* nvg, Rectangle<float> const b) const
    {
        auto const label = state && textOn.toString().isNotEmpty() ? textOn.toString() : text.toString();
        if (label.isEmpty())
            return;

        auto const align = getValue<int>(textAlign);
        NVGScopedState scopedState(nvg);
        nvgIntersectScissor(nvg, b.getX(), b.getY(), b.getWidth(), b.getHeight());
        nvgFontFace(nvg, "Inter-Regular");
        nvgFontSize(nvg, static_cast<float>(std::max(getValue<int>(fontSize), 4)));
        nvgTextAlign(nvg, NVG_ALIGN_MIDDLE | (align == 1 ? NVG_ALIGN_LEFT : align == 3 ? NVG_ALIGN_RIGHT : NVG_ALIGN_CENTER));
        nvgFillColor(nvg, nvgColour(getShownTextColour(state)));
        auto const x = align == 1 ? b.getX() + 4.0f : align == 3 ? b.getRight() - 4.0f : b.getCentreX();
        nvgText(nvg, x, b.getCentreY(), label.toRawUTF8(), nullptr);
    }

    void onConstrainerCreate() override
    {
        constrainer->setMinimumHeight(9);
        constrainer->setMinimumWidth(9);
    }

    void updateColours()
    {
        bgCol = nvgColour(Colour::fromString(secondaryColour.toString()));
        fgCol = nvgColour(Colour::fromString(primaryColour.toString()));
        repaint();
    }

    void update() override
    {
        if (auto button = ptr.get<t_fake_button>()) {
            primaryColour = String::fromUTF8(button->x_fg->s_name).replace("#", "ff");
            secondaryColour = String::fromUTF8(button->x_bg->s_name).replace("#", "ff");
            transparent = button->x_transparent;
            sizeProperty = VarArray(button->x_w, button->x_h);
            readOnly = button->x_readonly;
            oval = button->x_oval;
            ovalOutline = button->x_oval_outline;
            // PgD4S: lit as Pd has it (set by the patch before this view opened, a loadbang)
            if (button->x_mode != 2)
                state = button->x_state != 0;
            if (button->x_mode == 0) {
                mode = Latch;
            } else if (button->x_mode == 1) {
                mode = Toggle;
            } else if (button->x_mode == 2) {
                mode = Bang;
            } else {
                mode = Click;
            }
        }

        updateColours();
        readAttributes();
    }

    void toggleObject(Point<int> position) override
    {
        if (!alreadyTriggered) {

            if (mode == Latch) {
                state = true;
            } else if (mode == Toggle) {
                state = !state;
            }

            if (mode == Bang) {
                state = true;
                if (auto button = ptr.get<t_fake_button>()) {
                    outlet_bang(button->x_obj.ob_outlet);
                }
                Timer::callAfterDelay(250,
                    [_this = SafePointer(this)]() mutable {
                        // First check if this object still exists
                        if (!_this)
                            return;

                        if (_this->state) {
                            _this->state = false;
                            _this->repaint();
                        }
                    });
            } else if (mode != Click) {
                if (auto button = ptr.get<t_fake_button>()) {
                    button->x_state = state;
                    outlet_float(button->x_obj.ob_outlet, state);
                }
            }
            repaint();
            alreadyTriggered = true;
        }
    }
    void untoggleObject() override
    {
        if (alreadyTriggered) {
            if (mode == Latch) {
                state = false;
                if (auto button = ptr.get<t_fake_button>()) {
                    button->x_state = 0;
                    outlet_float(button->x_obj.ob_outlet, 0);
                }
            }
            repaint();
            alreadyTriggered = false;
        }
    }

    Rectangle<int> getPdBounds() override
    {
        if (auto gobj = ptr.get<t_gobj>()) {
            auto* patch = cnv->patch.getRawPointer();
            int x = 0, y = 0, w = 0, h = 0;
            pd::Interface::getObjectBounds(patch, gobj.get(), &x, &y, &w, &h);

            return Rectangle<int>(x, y, w + 1, h + 1);
        }

        return { };
    }

    void setPdBounds(Rectangle<int> const b) override
    {
        if (auto button = ptr.get<t_fake_button>()) {
            auto* patch = cnv->patch.getRawPointer();
            pd::Interface::moveObject(patch, button.cast<t_gobj>(), b.getX(), b.getY());
            button->x_w = b.getWidth() - 1;
            button->x_h = b.getHeight() - 1;
        }
    }

    void updateSizeProperty() override
    {
        setPdBounds(object->getObjectBounds());

        if (auto button = ptr.get<t_fake_button>()) {
            setParameterExcludingListener(sizeProperty, VarArray(var(button->x_w), var(button->x_h)));
        }
    }

    void mouseEnter(MouseEvent const& e) override
    {
        if (::getValue<bool>(cnv->locked)) {
            if (auto button = ptr.get<t_fake_button>()) {
                if (button->x_snd_hover->s_thing && button->x_snd_hover != gensym("empty") && button->x_snd_hover != pd->generateSymbol(""))
                    pd_float(button->x_snd_hover->s_thing, 1.0f);
            }
        }
    }

    void mouseExit(MouseEvent const& e) override
    {
        if (::getValue<bool>(cnv->locked)) {
            if (auto button = ptr.get<t_fake_button>()) {
                if (button->x_snd_hover->s_thing && button->x_snd_hover != gensym("empty") && button->x_snd_hover != pd->generateSymbol(""))
                    pd_float(button->x_snd_hover->s_thing, 0.0f);
            }
        }
    }

    void mouseDown(MouseEvent const& e) override
    {
        if (!e.mods.isLeftButtonDown())
            return;

        if (mode == Latch) {
            state = true;
        } else if (mode == Toggle) {
            state = !state;
        }

        if (mode == Click) {
            state = true;
            if (auto button = ptr.get<t_fake_button>()) {
                t_atom list[4];
                SETFLOAT(list, state);
                SETFLOAT(list + 1, e.getNumberOfClicks() > 1);
                SETFLOAT(list + 2, e.mods.isShiftDown());
                SETFLOAT(list + 3, e.mods.isCommandDown() || e.mods.isAltDown());
                outlet_list(button->x_obj.ob_outlet, pd->generateSymbol("list"), 4, list);
            }
        } else if (mode == Bang) {
            state = true;
            if (auto button = ptr.get<t_fake_button>()) {
                outlet_bang(button->x_obj.ob_outlet);
                if (button->x_snd->s_thing && !button->x_edit && button->x_snd != gensym("empty") && button->x_snd != pd->generateSymbol(""))
                    pd_bang(button->x_snd->s_thing);
            }
            Timer::callAfterDelay(250,
                [_this = SafePointer(this)]() mutable {
                    // First check if this object still exists
                    if (!_this)
                        return;

                    if (_this->state) {
                        _this->state = false;
                        _this->repaint();
                    }
                });
        } else {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_state = state;
                outlet_float(button->x_obj.ob_outlet, state);
                if (button->x_snd->s_thing && !button->x_edit && button->x_snd != gensym("empty") && button->x_snd != pd->generateSymbol(""))
                    pd_float(button->x_snd->s_thing, state);
            }
        }

        // Make sure we don't re-click with an accidental drag
        alreadyTriggered = true;

        repaint();
    }

    bool canReceiveMouseEvent(int const x, int const y) override
    {
        return !getValue<bool>(readOnly);
    }

    void mouseUp(MouseEvent const& e) override
    {
        if (!e.mods.isLeftButtonDown())
            return;

        alreadyTriggered = false;
        if (mode == Click) {
            state = false;
            if (auto button = ptr.get<t_fake_button>()) {
                t_atom list[4];
                SETFLOAT(list, 0);
                SETFLOAT(list + 1, e.getNumberOfClicks() > 1);
                SETFLOAT(list + 2, e.mods.isShiftDown());
                SETFLOAT(list + 3, e.mods.isCommandDown() || e.mods.isAltDown());
                outlet_list(button->x_obj.ob_outlet, pd->generateSymbol("list"), 4, list);
            }
        } else if (mode == Latch) {
            state = false;
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_state = 0;
                outlet_float(button->x_obj.ob_outlet, 0);
                if (button->x_snd->s_thing && !button->x_edit && button->x_snd != gensym("empty") && button->x_snd != pd->generateSymbol(""))
                    pd_float(button->x_snd->s_thing, 0);
            }
        }

        repaint();
    }

    void render(NVGcontext* nvg) override
    {
        auto const b = getLocalBounds().toFloat();

        auto fillColour = getValue<bool>(transparent) ? nvgRGBA(0, 0, 0, 0) : bgCol;
        // An oval button's outline is round too ("Oval outline", on by default, as ELSE draws it in Pd), so Live's
        // round live.button, converted, is round lit or not
        auto const roundOutline = getValue<bool>(oval) && getValue<bool>(ovalOutline);
        auto const boxRadius = roundOutline ? std::min(b.getWidth(), b.getHeight()) * 0.5f : Corners::objectCornerRadius;
        nvgDrawRoundedRect(nvg, b.getX(), b.getY(), b.getWidth(), b.getHeight(), fillColour, object->isSelected() ? nvgColour(PlugDataColours::objectSelectedOutlineColour) : nvgColour(PlugDataColours::objectOutlineColour), boxRadius);

        auto radius = getValue<bool>(oval) ? getWidth() : Corners::objectCornerRadius;

        // PgD4S: a text button (live.text) is lit edge to edge, its text over it
        if (hasText()) {
            if (state && !getValue<bool>(transparent)) {
                auto const lit = b.reduced(1);
                nvgDrawRoundedRect(nvg, lit.getX(), lit.getY(), lit.getWidth(), lit.getHeight(), fgCol, fgCol, std::max(boxRadius - 1.0f, 0.0f));
            }
            renderText(nvg, b.reduced(1));
            return;
        }

        if (!getValue<bool>(transparent)) {
            auto const guiBounds = b.reduced(1);
            auto const outerBounds = guiBounds.reduced(5);
            auto const innerRectBounds = outerBounds.reduced(2.5);
            bool spaceToShowRect = false;

            if (b.getWidth() >= 25 && b.getHeight() >= 25) {
                spaceToShowRect = true;
                nvgDrawRoundedRect(nvg, outerBounds.getX(), outerBounds.getY(), outerBounds.getWidth(), outerBounds.getHeight(), nvgColour(PlugDataColours::guiObjectInternalOutlineColour), nvgColour(PlugDataColours::guiObjectInternalOutlineColour), radius);
                nvgDrawRoundedRect(nvg, innerRectBounds.getX(), innerRectBounds.getY(), innerRectBounds.getWidth(), innerRectBounds.getHeight(), bgCol, bgCol, radius - 1.0f);
            }

            // Fill ellipse if bangState is true
            if (state) {
                auto const innerBounds = spaceToShowRect ? innerRectBounds.reduced(1) : guiBounds;
                auto const cornerRadius = spaceToShowRect ? radius - 1.5f : radius - 1;
                nvgDrawRoundedRect(nvg, innerBounds.getX(), innerBounds.getY(), innerBounds.getWidth(), innerBounds.getHeight(), fgCol, fgCol, cornerRadius);
            }
        }
    }

    bool canEdgeOverrideAspectRatio() override
    {
        return true;
    }

    void propertyChanged(Value& value) override
    {
        if (value.refersToSameSourceAs(sizeProperty)) {
            auto* constrainer = getConstrainer();

            auto const& arr = *sizeProperty.getValue().getArray();
            auto const width = std::max(static_cast<int>(arr[0]), constrainer->getMinimumWidth());
            auto height = std::max(static_cast<int>(arr[1]), constrainer->getMinimumHeight());

            setParameterExcludingListener(sizeProperty, VarArray(width, height));
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_w = width;
                button->x_h = height;
            }
            object->updateBounds();
        } else if (value.refersToSameSourceAs(primaryColour)) {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_fg = pd->generateSymbol("#" + primaryColour.toString().substring(2));
            }
            updateColours();
        } else if (value.refersToSameSourceAs(secondaryColour)) {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_bg = pd->generateSymbol("#" + secondaryColour.toString().substring(2));
            }
            updateColours();
        }
        if (value.refersToSameSourceAs(transparent)) {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_transparent = getValue<bool>(transparent);
            }
            repaint();
        }
        if (value.refersToSameSourceAs(oval)) {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_oval = getValue<bool>(oval);
            }
            repaint();
        }
        if (value.refersToSameSourceAs(ovalOutline)) {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_oval_outline = getValue<bool>(ovalOutline);
            }
            repaint();
        }
        if (value.refersToSameSourceAs(readOnly)) {
            if (auto button = ptr.get<t_fake_button>()) {
                button->x_readonly = getValue<bool>(readOnly);
            }
        }
        if (value.refersToSameSourceAs(text)) {
            setAttribute("text", text.toString());
            repaint();
        } else if (value.refersToSameSourceAs(textOn)) {
            setAttribute("texton", textOn.toString());
            repaint();
        } else if (value.refersToSameSourceAs(textColour)) {
            setTextColour = Colour::fromString(textColour.toString());
            setAttribute("textcolor", toHex(*setTextColour));
            repaint();
        } else if (value.refersToSameSourceAs(textOnColour)) {
            setTextOnColour = Colour::fromString(textOnColour.toString());
            setAttribute("textoncolor", toHex(*setTextOnColour));
            repaint();
        } else if (value.refersToSameSourceAs(fontSize)) {
            setAttribute("fontsize", String(std::max(getValue<int>(fontSize), 4)));
            repaint();
        } else if (value.refersToSameSourceAs(textAlign)) {
            auto const align = getValue<int>(textAlign);
            setAttribute("align", align == 2 ? String() : alignNames[std::clamp(align - 1, 0, 2)]);
            repaint();
        }
    }

    void receiveObjectMessage(hash32 const symbol, SmallArray<pd::Atom> const& atoms) override
    {
        switch (symbol) {
        case hash("bgcolor"): {
            if (atoms.size() >= 3) {
                setParameterExcludingListener(secondaryColour, Colour(atoms[0].getFloat(), atoms[1].getFloat(), atoms[2].getFloat()).toString());
                updateColours();
            }
            break;
        }
        case hash("fgcolor"): {
            if (atoms.size() >= 3) {
                setParameterExcludingListener(primaryColour, Colour(atoms[0].getFloat(), atoms[1].getFloat(), atoms[2].getFloat()).toString());
                updateColours();
            }
            break;
        }
        case hash("readonly"): {
            if (atoms.size() >= 1) {
                setParameterExcludingListener(readOnly, atoms[0].getFloat());
            }
            break;
        }
        case hash("transparent"): {
            if (atoms.size() >= 1) {
                setParameterExcludingListener(transparent, atoms[0].getFloat());
                repaint();
            }
            break;
        }
        case hash("oval"): {
            if (atoms.size() >= 1) {
                setParameterExcludingListener(oval, atoms[0].getFloat());
                repaint();
            }
            break;
        }
        case hash("ovaloutline"): {
            if (atoms.size() >= 1) {
                setParameterExcludingListener(ovalOutline, atoms[0].getFloat());
                repaint();
            }
            break;
        }
        case hash("float"): {
            if (atoms.size()) {
                state = !approximatelyEqual(atoms[0].getFloat(), 0.0f);
            }
            repaint();
            break;
        }
        case hash("set"): {
            // PgD4S: lit or not without output (ELSE's button_set), e.g. Snake's selectors turning the
            // others off
            if (mode != Bang && atoms.size() && atoms[0].isFloat()) {
                state = !approximatelyEqual(atoms[0].getFloat(), 0.0f);
                repaint();
            }
            break;
        }
        case hash("click"):
        case hash("latch"):
        case hash("bang"):
        case hash("toggle"): {
            update();
            break;
        }
        default:
            break;
        }
    }
};
