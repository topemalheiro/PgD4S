/*
 // Copyright (c) 2021-2025 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */
#pragma once
#include "Components/DraggableNumber.h"
#include "ObjectBase.h"
#include "IEMHelper.h"

class NumberObject final : public ObjectBase {
    DraggableNumber input;
    IEMHelper iemHelper;

    Value widthProperty = SynchronousValue();
    Value heightProperty = SynchronousValue();
    Value min = SynchronousValue(-std::numeric_limits<float>::infinity());
    Value max = SynchronousValue(std::numeric_limits<float>::infinity());
    Value logHeight = SynchronousValue();
    Value logMode = SynchronousValue();

    // PgD4S: live.numbox's looks, stored as attributes ("#X attr")
    Value showArrow = SynchronousValue();
    Value fillBox = SynchronousValue();
    Value decimals = SynchronousValue();
    Value unitText = SynchronousValue();
    Value alignment = SynchronousValue();
    Value showText = SynchronousValue(); // PgD4S: "#X attr N hidetext 1" (Max's text colour with alpha 0)

    float preFocusValue;
    float value = 0.0f;
    bool inactive = false; // PgD4S: "active 0" (Max's live.numbox): greyed out, no mouse

    NVGcolor backgroundCol;
    NVGcolor foregroundCol;
    NVGcolor flagCol;

public:
    NumberObject(pd::WeakReference ptr, Object* object)
        : ObjectBase(ptr, object)
        , input(false)
        , iemHelper(ptr, object, this)

    {
        iemHelper.iemColourChangedCallback = [this] {
            // We use this callback to be informed when the IEM colour has changed.
            // As getBackgroundColour() will lock audio thread!
            backgroundCol = nvgColour(iemHelper.getBackgroundColour());

            foregroundCol = nvgColour(iemHelper.getForegroundColour());
            flagCol = nvgColour(iemHelper.getForegroundColour());

            input.setColour(Label::textColourId, iemHelper.getForegroundColour());
            input.setColour(Label::textWhenEditingColourId, iemHelper.getBackgroundColour().contrasting());
        };

        input.onEditorShow = [this] {
            auto* editor = input.getCurrentTextEditor();
            startEdition();

            editor->setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);
            editor->setBorder({ 0, 8, 4, 1 });
        };

        input.onInteraction = [this](bool const isFocused) {
            if (isFocused)
                input.setColour(Label::textColourId, iemHelper.getBackgroundColour().contrasting());
            else
                input.setColour(Label::textColourId, iemHelper.getForegroundColour());
        };

        input.setEditableOnClick(false, false, true);
        input.onEditorHide = [this] {
            stopEdition();
        };

        input.setBorderSize({ 1, 12, 1, 0 });

        addAndMakeVisible(input);

        addMouseListener(this, true);

        input.dragStart = [this] {
            startEdition();
        };

        input.onValueChange = [this](float const newValue) {
            sendFloatValue(newValue);
        };

        input.onReturnKey = [this](double const newValue) {
            sendFloatValue(newValue);
        };

        input.dragEnd = [this] {
            stopEdition();
        };

        objectParameters.addParamInt("Width (chars)", cDimensions, &widthProperty, var(), true, 1);
        objectParameters.addParamInt("Height", cDimensions, &heightProperty, var(), true, 8);
        objectParameters.addParamInt("Text/Label Height", cDimensions, &iemHelper.labelHeight, 10, true, 1);
        objectParameters.addParamFloat("Minimum", cGeneral, &min, -9.999999933815813e36);
        objectParameters.addParamFloat("Maximum", cGeneral, &max, 9.999999933815813e36);
        objectParameters.addParamBool("Logarithmic mode", cGeneral, &logMode, { "Off", "On" }, var(false));
        objectParameters.addParamInt("Logarithmic height", cGeneral, &logHeight, var(256), true, 1);
        objectParameters.addParamColourFG(&iemHelper.primaryColour);
        objectParameters.addParamColourBG(&iemHelper.secondaryColour);
        objectParameters.addParamReceiveSymbol(&iemHelper.receiveSymbol);
        objectParameters.addParamSendSymbol(&iemHelper.sendSymbol);
        objectParameters.addParamString("Text", cLabel, &iemHelper.labelText, "");
        objectParameters.addParamColourLabel(&iemHelper.labelColour);
        objectParameters.addParamRange("Position", cLabel, &iemHelper.labelPosition, { 0, -8 });
        objectParameters.addParamBool("Initialise", cGeneral, &iemHelper.initialise, { "No", "Yes" }, 0);
        objectParameters.addParamBool("Arrow", cAppearance, &showArrow, { "No", "Yes" }, 1);
        objectParameters.addParamBool("Background", cAppearance, &fillBox, { "No", "Yes" }, 1);
        objectParameters.addParamBool("Show number", cAppearance, &showText, { "No", "Yes" }, 1);
        objectParameters.addParamCombo("Decimals", cAppearance, &decimals, { "Auto", "0", "1", "2", "3", "4" }, 1);
        objectParameters.addParamString("Unit", cAppearance, &unitText, "");
        objectParameters.addParamCombo("Alignment", cAppearance, &alignment, { "Left", "Centred", "Right" }, 1);

        input.setResetValue(0.0f);
    }

    static inline StringArray const alignNames { "left", "center", "right" };

    // PgD4S: the attributes behind live.numbox's looks; set without the listener, so reading never
    // writes them back
    void readAttributes()
    {
        setParameterExcludingListener(showArrow, getAttribute("arrow", "1").getIntValue() != 0);
        setParameterExcludingListener(fillBox, getAttribute("fill", "1").getIntValue() != 0);
        setParameterExcludingListener(decimals, hasAttribute("decimals") ? std::clamp(getAttribute("decimals").getIntValue(), 0, 4) + 2 : 1);
        setParameterExcludingListener(unitText, getAttribute("unit"));
        setParameterExcludingListener(alignment, std::max(1, alignNames.indexOf(getAttribute("align")) + 1));
        setParameterExcludingListener(showText, getAttribute("hidetext", "0").getIntValue() == 0);
        updateDisplayFormat();
    }

    void updateDisplayFormat()
    {
        auto const align = getValue<int>(alignment);
        auto const justification = align == 2 ? Justification::centred : align == 3 ? Justification::centredRight : Justification::centredLeft;
        input.setDisplayFormat(getValue<int>(decimals) - 2, unitText.toString(), justification);
        input.setBorderSize({ 1, getValue<bool>(showArrow) ? 12 : 2, 1, getValue<bool>(showArrow) ? 0 : 2 });
        repaint();
    }

    void update() override
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            widthProperty = var(nbx->x_numwidth);
            heightProperty = var(nbx->x_gui.x_h);
            logMode = nbx->x_lin0_log1;
            logHeight = nbx->x_log_height;
            min = nbx->x_min;
            max = nbx->x_max;
            value = nbx->x_val;
            inactive = nbx->x_inactive;
        }

        input.setValue(value, dontSendNotification, false);
        input.setMinimum(getValue<float>(min));
        input.setMaximum(getValue<float>(max));
        input.setLogarithmicHeight(getValue<float>(logHeight));
        input.setDragMode(getValue<bool>(logMode) ? DraggableNumber::Logarithmic : DraggableNumber::Regular);

        iemHelper.update();

        auto const fontHeight = getValue<int>(iemHelper.labelHeight);
        input.setFont(Fonts::getTabularNumbersFont().withHeight(fontHeight + 3.0f));
        readAttributes();
        input.setInterceptsMouseClicks(!inactive, !inactive);
        updateLabel();
    }

    bool hideInlet() override
    {
        return iemHelper.hasReceiveSymbol();
    }

    bool hideOutlet() override
    {
        return iemHelper.hasSendSymbol();
    }

    void updateLabel() override
    {
        iemHelper.updateLabel(labels);
    }

    Rectangle<int> getPdBounds() override
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            auto* patch = cnv->patch.getRawPointer();

            int x = 0, y = 0, w = 0, h = 0;
            pd::Interface::getObjectBounds(patch, nbx.cast<t_gobj>(), &x, &y, &w, &h);

            return { x, y, calcFontWidth(std::max(nbx->x_numwidth, 1)) + 1, h + 1 };
        }

        return { };
    }

    int getFontWidth() const
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            return nbx->x_gui.x_fontsize;
        }
        return 10;
    }

    void setPdBounds(Rectangle<int> const b) override
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            auto* patchPtr = cnv->patch.getRawPointer();

            pd::Interface::moveObject(patchPtr, nbx.cast<t_gobj>(), b.getX(), b.getY());

            nbx->x_numwidth = calcNumWidth(b.getWidth() - 1);
            nbx->x_gui.x_w = b.getWidth() - 1;
            nbx->x_gui.x_h = b.getHeight() - 1;
        }
    }

    void updateSizeProperty() override
    {
        setPdBounds(object->getObjectBounds());

        if (auto nbx = ptr.get<t_my_numbox>()) {
            setParameterExcludingListener(widthProperty, var(nbx->x_numwidth));
            setParameterExcludingListener(heightProperty, var(nbx->x_gui.x_h));
        }
    }

    void resized() override
    {
        input.setBounds(getLocalBounds());
    }

    void focusGained(FocusChangeType cause) override
    {
        preFocusValue = value;
        repaint();
    }

    bool keyPressed(KeyPress const& key) override
    {
        if (key.getKeyCode() == KeyPress::returnKey) {
            auto const inputValue = input.getText().getFloatValue();
            preFocusValue = value;
            sendFloatValue(inputValue);
            return true;
        }

        return false;
    }

    void focusLost(FocusChangeType cause) override
    {
        auto const inputValue = input.getText().getFloatValue();
        if (!approximatelyEqual(inputValue, preFocusValue)) {
            sendFloatValue(inputValue);
        }
        repaint();
    }

    void focusOfChildComponentChanged(FocusChangeType cause) override
    {
        repaint();
    }

    void lock(bool const isLocked) override
    {
        input.setResetEnabled(getValue<bool>(cnv->locked));
        setInterceptsMouseClicks(isLocked, isLocked);
        repaint();
    }

    bool canReceiveMouseEvent(int x, int y) override
    {
        return !inactive;
    }

    void receiveObjectMessage(hash32 const symbol, SmallArray<pd::Atom> const& atoms) override
    {
        switch (symbol) {
        case hash("active"): {
            if (auto nbx = ptr.get<t_my_numbox>())
                inactive = nbx->x_inactive;
            input.setInterceptsMouseClicks(!inactive, !inactive);
            repaint();
            break;
        }
        case hash("float"):
        case hash("list"):
        case hash("set"): {
            if (atoms.size() > 0 && atoms[0].isFloat()) {
                value = atoms[0].getFloat();
                input.setValue(value, dontSendNotification, false);
            }
            break;
        }
        case hash("range"): {
            if (atoms.size() >= 2) {
                if (auto nbx = ptr.get<t_my_numbox>()) {
                    min = nbx->x_min;
                    max = nbx->x_max;
                }
            }
            break;
        }
        case hash("log"): {
            setParameterExcludingListener(logMode, true);
            input.setDragMode(DraggableNumber::Logarithmic);
            break;
        }
        case hash("lin"): {
            setParameterExcludingListener(logMode, false);
            input.setDragMode(DraggableNumber::Regular);
            break;
        }
        case hash("log_height"): {
            auto const height = static_cast<int>(atoms[0].getFloat());
            setParameterExcludingListener(logHeight, height);
            input.setLogarithmicHeight(height);
        }
        default: {
            iemHelper.receiveObjectMessage(symbol, atoms);
            break;
        }
        }
    }

    int calcFontWidth(int const numWidth) const
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            int f = 31;

            if (nbx->x_gui.x_fsf.x_font_style == 1)
                f = 27;
            else if (nbx->x_gui.x_fsf.x_font_style == 2)
                f = 25;

            int w = nbx->x_gui.x_fontsize * f * numWidth;
            w /= 36;
            return w + nbx->x_gui.x_h / 2 + 4;
        }
        return 14;
    }

    int calcNumWidth(int const width) const
    {
        if (auto nbx = ptr.get<t_my_numbox>()) {
            int f = 31;
            if (nbx->x_gui.x_fsf.x_font_style == 1)
                f = 27;
            else if (nbx->x_gui.x_fsf.x_font_style == 2)
                f = 25;

            return -(18.0f * (8.0f + nbx->x_gui.x_h - 2 * width)) / (nbx->x_gui.x_fontsize * f) + 1;
        }
        return 1;
    }

    void propertyChanged(Value& value) override
    {
        if (value.refersToSameSourceAs(showArrow)) {
            setAttribute("arrow", getValue<bool>(showArrow) ? "" : "0");
            updateDisplayFormat();
        } else if (value.refersToSameSourceAs(fillBox)) {
            setAttribute("fill", getValue<bool>(fillBox) ? "" : "0");
            updateDisplayFormat();
        } else if (value.refersToSameSourceAs(decimals)) {
            auto const fixed = getValue<int>(decimals) - 2;
            setAttribute("decimals", fixed >= 0 ? String(fixed) : String());
            updateDisplayFormat();
        } else if (value.refersToSameSourceAs(unitText)) {
            setAttribute("unit", unitText.toString());
            updateDisplayFormat();
        } else if (value.refersToSameSourceAs(showText)) {
            setAttribute("hidetext", getValue<bool>(showText) ? "" : "1");
            repaint();
        } else if (value.refersToSameSourceAs(alignment)) {
            auto const align = getValue<int>(alignment);
            setAttribute("align", align > 1 ? alignNames[align - 1] : "");
            updateDisplayFormat();
        } else if (value.refersToSameSourceAs(widthProperty)) {
            auto const numWidth = std::max(getValue<int>(widthProperty), 1);
            auto const width = calcFontWidth(numWidth) + 1;

            setParameterExcludingListener(widthProperty, var(numWidth));

            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_numwidth = numWidth;
                nbx->x_gui.x_w = width;
            }

            object->updateBounds();
        } else if (value.refersToSameSourceAs(heightProperty)) {
            auto const height = std::max(getValue<int>(heightProperty), constrainer->getMinimumHeight());
            setParameterExcludingListener(heightProperty, var(height));
            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_gui.x_h = height;
            }
            object->updateBounds();
        } else if (value.refersToSameSourceAs(min)) {
            input.setMinimum(getValue<float>(min));
            if (auto numbox = ptr.get<t_my_numbox>()) {
                numbox->x_min = getValue<float>(min);
            }
        } else if (value.refersToSameSourceAs(max)) {
            input.setMaximum(getValue<float>(max));
            if (auto numbox = ptr.get<t_my_numbox>()) {
                numbox->x_max = getValue<float>(max);
            }
        } else if (value.refersToSameSourceAs(logHeight)) {
            auto const height = getValue<int>(logHeight);
            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_log_height = height;
            }

            input.setLogarithmicHeight(height);
        } else if (value.refersToSameSourceAs(logMode)) {
            auto const logarithmicDrag = getValue<bool>(logMode);
            if (auto nbx = ptr.get<t_my_numbox>()) {
                nbx->x_lin0_log1 = logarithmicDrag;
            }
            input.setDragMode(logarithmicDrag ? DraggableNumber::Logarithmic : DraggableNumber::Regular);
        } else if (value.refersToSameSourceAs(iemHelper.labelHeight)) {
            auto const fontHeight = getValue<int>(iemHelper.labelHeight);
            iemHelper.setFontHeight(fontHeight);
            updateLabel();

            input.setFont(Fonts::getTabularNumbersFont().withHeight(fontHeight + 3.0f));
            object->updateBounds();
        } else {
            iemHelper.valueChanged(value);
        }
    }

    void render(NVGcontext* nvg) override
    {
        auto const b = getLocalBounds().toFloat();

        // PgD4S: inactive ("active 0") draws greyed out
        NVGScopedState inactiveState(nvg);
        if (inactive)
            nvgGlobalAlpha(nvg, 0.4f);

        bool const selected = object->isSelected() && !cnv->isGraph;

        // PgD4S: without a background, just the text (an outline only while editing the patch)
        if (getValue<bool>(fillBox)) {
            nvgDrawRoundedRect(nvg, b.getX(), b.getY(), b.getWidth(), b.getHeight(), backgroundCol, nvgColour(selected ? PlugDataColours::objectSelectedOutlineColour : PlugDataColours::objectOutlineColour), Corners::objectCornerRadius);
        } else if (selected || !getValue<bool>(object->locked)) {
            nvgDrawRoundedRect(nvg, b.getX(), b.getY(), b.getWidth(), b.getHeight(), nvgRGBA(0, 0, 0, 0), nvgColour(selected ? PlugDataColours::objectSelectedOutlineColour : PlugDataColours::objectOutlineColour.withAlpha(0.5f)), Corners::objectCornerRadius);
        }

        // PgD4S: hidden digits show only while typing a number
        bool const drawText = getValue<bool>(showText) || input.isBeingEdited();

        if (!getValue<bool>(showArrow)) {
            if (drawText)
                input.render(nvg, cnv->editor->getNanoLLGC());
            return;
        }

        constexpr float indent = 9.0f;
        Rectangle<float> const iconBounds = { (b.getX() + 4.0f), (b.getY() + 4.0f), (indent - 4.0f), (b.getHeight() - 8.0f) };

        auto const centreY = iconBounds.getCentreY();
        auto const leftX = iconBounds.getX();
        nvgBeginPath(nvg);
        nvgMoveTo(nvg, leftX, centreY + 5.0f);
        nvgLineTo(nvg, iconBounds.getRight(), centreY);
        nvgLineTo(nvg, leftX, centreY - 5.0f);
        nvgClosePath(nvg);

        bool const highlighted = hasKeyboardFocus(true) && getValue<bool>(object->locked);
        auto const flagCol = highlighted ? nvgColour(PlugDataColours::objectSelectedOutlineColour) : nvgColour(PlugDataColours::guiObjectInternalOutlineColour);

        nvgFillColor(nvg, flagCol);
        nvgFill(nvg);

        if (drawText)
            input.render(nvg, cnv->editor->getNanoLLGC());
    }

    std::unique_ptr<ComponentBoundsConstrainer> createConstrainer() override
    {
        class NumboxBoundsConstrainer : public ComponentBoundsConstrainer {
        public:
            Object* object;
            NumberObject* numbox;

            NumboxBoundsConstrainer(Object* parent, NumberObject* nbx)
                : object(parent)
                , numbox(nbx)
            {
            }

            void checkBounds(Rectangle<int>& bounds,
                Rectangle<int> const& old,
                Rectangle<int> const& limits,
                bool isStretchingTop,
                bool const isStretchingLeft,
                bool isStretchingBottom,
                bool isStretchingRight) override
            {
                auto const oldBounds = old.reduced(Object::margin);
                auto const newBounds = bounds.reduced(Object::margin);

                // Calculate the width in text characters for both
                auto const newCharWidth = numbox->calcNumWidth(newBounds.getWidth() - 1);

                // Set new width
                if (auto nbx = numbox->ptr.get<t_my_numbox>()) {
                    nbx->x_numwidth = newCharWidth;
                    nbx->x_gui.x_h = std::max(newBounds.getHeight(), 8);
                }

                bounds = object->gui->getPdBounds().expanded(Object::margin) + object->cnv->canvasOrigin;

                // If we're resizing the left edge, move the object left
                if (isStretchingLeft) {
                    auto const x = oldBounds.getRight() - (bounds.getWidth() - Object::doubleMargin);
                    auto const y = oldBounds.getY(); // don't allow y resize

                    if (auto nbx = numbox->ptr.get<t_my_numbox>()) {
                        auto* patch = object->cnv->patch.getRawPointer();
                        pd::Interface::moveObject(patch, nbx.cast<t_gobj>(), x - object->cnv->canvasOrigin.x, y - object->cnv->canvasOrigin.y);
                    }
                    bounds = object->gui->getPdBounds().expanded(Object::margin) + object->cnv->canvasOrigin;
                }
            }
        };

        return std::make_unique<NumboxBoundsConstrainer>(object, this);
    }
};
