/*
 // PgD4S (plugdata for Spite): [spite.tab], Max for Live's live.tab.
 // A grid of cells with text or pictures; one is selected, a click selects and outputs its index.
 // The Pd side (pure-data/src/x_spite.c) keeps the state and the save format.
 */
#pragma once

extern "C" {
#include <x_spite.h>
}

class SpiteTabObject final : public ObjectBase {

    Value sizeProperty = SynchronousValue();
    Value items = SynchronousValue();
    Value pictures = SynchronousValue();
    Value rows = SynchronousValue();
    Value columns = SynchronousValue();
    Value initial = SynchronousValue();
    Value loadInit = SynchronousValue();
    Value spacing = SynchronousValue();
    Value roundness = SynchronousValue();
    Value fontSize = SynchronousValue();
    Value backgroundColour = SynchronousValue();
    Value foregroundColour = SynchronousValue();
    Value selectedBackgroundColour = SynchronousValue();
    Value selectedForegroundColour = SynchronousValue();
    Value sendSymbol = SynchronousValue();
    Value receiveSymbol = SynchronousValue();

    int selected = 0;
    StringArray itemTexts;
    StringArray pictureFiles;
    OwnedArray<Drawable> drawables;
    SmallArray<bool> drawableIsVector;

    NVGImage image;
    bool needsRedraw = true;

public:
    SpiteTabObject(pd::WeakReference obj, Object* parent)
        : ObjectBase(obj, parent)
    {
        objectParameters.addParamSize(&sizeProperty);
        objectParameters.addParamString("Items", cGeneral, &items, "");
        objectParameters.addParamString("Pictures", cGeneral, &pictures, "");
        objectParameters.addParamInt("Rows", cGeneral, &rows, 1, true, 0);
        objectParameters.addParamInt("Columns", cGeneral, &columns, 0, true, 0);
        objectParameters.addParamInt("Initial", cGeneral, &initial, 0, true, 0);
        objectParameters.addParamBool("Output on load", cGeneral, &loadInit, { "No", "Yes" }, 0);
        objectParameters.addParamReceiveSymbol(&receiveSymbol);
        objectParameters.addParamSendSymbol(&sendSymbol);

        objectParameters.addParamColour("Background", cAppearance, &backgroundColour, PlugDataColour::guiObjectBackgroundColourId);
        objectParameters.addParamColour("Foreground", cAppearance, &foregroundColour, PlugDataColour::canvasTextColourId);
        objectParameters.addParamColour("Selected background", cAppearance, &selectedBackgroundColour, PlugDataColour::guiObjectInternalOutlineColourId);
        objectParameters.addParamColour("Selected foreground", cAppearance, &selectedForegroundColour, PlugDataColour::canvasTextColourId);
        objectParameters.addParamInt("Spacing", cAppearance, &spacing, 2, true, 0);
        objectParameters.addParamInt("Corner radius", cAppearance, &roundness, 3, true, 0);
        objectParameters.addParamInt("Font size", cAppearance, &fontSize, 10, true, 4);
    }

    // Pd-style word list: spaces separate, "\ " is a space inside a word
    static StringArray splitWords(String const& text)
    {
        StringArray words;
        String current;
        for (int i = 0; i < text.length(); i++) {
            auto const c = text[i];
            if (c == '\\' && i + 1 < text.length() && text[i + 1] == ' ') {
                current << ' ';
                i++;
            } else if (c == ' ') {
                if (current.isNotEmpty())
                    words.add(current);
                current.clear();
            } else {
                current << String::charToString(c);
            }
        }
        if (current.isNotEmpty())
            words.add(current);
        return words;
    }

    static String joinWords(StringArray const& words)
    {
        StringArray escaped;
        for (auto const& word : words)
            escaped.add(word.replace(" ", "\\ "));
        return escaped.joinIntoString(" ");
    }

    static String colourString(t_symbol const* s)
    {
        return String::fromUTF8(s->s_name).replace("#", "ff");
    }

    static String symbolString(t_symbol const* s)
    {
        auto const text = String::fromUTF8(s ? s->s_name : "");
        return text == "empty" ? String() : text;
    }

    void update() override
    {
        if (auto tab = ptr.get<t_spitetab>()) {
            sizeProperty = VarArray(tab->x_w, tab->x_h);
            rows = tab->x_rows;
            columns = tab->x_cols;
            initial = tab->x_init;
            loadInit = tab->x_loadinit;
            spacing = tab->x_spacing;
            roundness = tab->x_round;
            fontSize = tab->x_fontsize;
            backgroundColour = colourString(tab->x_bg);
            foregroundColour = colourString(tab->x_fg);
            selectedBackgroundColour = colourString(tab->x_onbg);
            selectedForegroundColour = colourString(tab->x_onfg);
            sendSymbol = symbolString(tab->x_snd_raw);
            receiveSymbol = symbolString(tab->x_rcv_raw);
            selected = tab->x_sel;

            itemTexts.clear();
            pictureFiles.clear();
            for (int i = 0; i < tab->x_n; i++) {
                itemTexts.add(String::fromUTF8(tab->x_items[i]->s_name));
                pictureFiles.add(String::fromUTF8(tab->x_pics[i]->s_name));
            }
            items = joinWords(itemTexts);
            StringArray pictureWords;
            for (auto const& file : pictureFiles)
                pictureWords.add(file.isEmpty() ? "-" : file);
            pictures = pictureFiles.joinIntoString("").isEmpty() ? String() : joinWords(pictureWords);
        }

        loadPictures();
        object->updateIolets();
        needsRedraw = true;
        repaint();
    }

    void loadPictures()
    {
        drawables.clear();
        drawableIsVector.clear();

        File directory;
        if (auto tab = ptr.get<t_spitetab>())
            directory = File(String::fromUTF8(canvas_getdir(tab->x_glist)->s_name));

        for (auto const& name : pictureFiles) {
            Drawable* drawable = nullptr;
            bool isVector = false;
            if (name.isNotEmpty()) {
                auto const file = File::isAbsolutePath(name) ? File(name) : directory.getChildFile(name);
                if (file.existsAsFile()) {
                    drawable = Drawable::createFromImageFile(file).release();
                    isVector = file.hasFileExtension("svg");
                }
            }
            drawables.add(drawable);
            drawableIsVector.add(isVector);
        }
    }

    bool hideInlet() override
    {
        return receiveSymbol.toString().isNotEmpty();
    }

    Rectangle<int> getPdBounds() override
    {
        if (auto gobj = ptr.get<t_gobj>()) {
            auto* patch = cnv->patch.getRawPointer();
            int x = 0, y = 0, w = 0, h = 0;
            pd::Interface::getObjectBounds(patch, gobj.get(), &x, &y, &w, &h);
            return { x, y, w + 1, h + 1 };
        }
        return {};
    }

    void setPdBounds(Rectangle<int> const b) override
    {
        if (auto tab = ptr.get<t_spitetab>()) {
            auto* patch = cnv->patch.getRawPointer();
            pd::Interface::moveObject(patch, tab.cast<t_gobj>(), b.getX(), b.getY());
            tab->x_w = std::max(b.getWidth() - 1, 8);
            tab->x_h = std::max(b.getHeight() - 1, 8);
        }
        needsRedraw = true;
    }

    void updateSizeProperty() override
    {
        setPdBounds(object->getObjectBounds());
        if (auto tab = ptr.get<t_spitetab>())
            setParameterExcludingListener(sizeProperty, VarArray(var(tab->x_w), var(tab->x_h)));
    }

    void onConstrainerCreate() override
    {
        constrainer->setMinimumSize(9, 9);
    }

    // The grid as laid out: rows and columns after "0 = as many as needed"
    void getGrid(int& r, int& c) const
    {
        r = getValue<int>(rows);
        c = getValue<int>(columns);
        auto const n = std::max(itemTexts.size(), 1);
        if (r <= 0 && c <= 0)
            r = 1;
        if (r <= 0)
            r = (n + c - 1) / c;
        if (c <= 0)
            c = (n + r - 1) / r;
    }

    Rectangle<float> getCellBounds(int const index, Rectangle<float> const area) const
    {
        int r, c;
        getGrid(r, c);
        auto const gap = static_cast<float>(getValue<int>(spacing));
        auto const cellW = (area.getWidth() - gap * (c - 1)) / c;
        auto const cellH = (area.getHeight() - gap * (r - 1)) / r;
        auto const row = index / c, col = index % c;
        return { area.getX() + col * (cellW + gap), area.getY() + row * (cellH + gap), cellW, cellH };
    }

    int getCellAt(Point<float> const position) const
    {
        auto const area = getLocalBounds().toFloat();
        for (int i = 0; i < itemTexts.size(); i++) {
            if (getCellBounds(i, area).expanded(getValue<int>(spacing) / 2.0f).contains(position))
                return i;
        }
        return -1;
    }

    void mouseDown(MouseEvent const& e) override
    {
        if (!e.mods.isLeftButtonDown())
            return;

        auto const index = getCellAt(e.position);
        if (index < 0)
            return;

        selected = index;
        needsRedraw = true;
        repaint();
        if (auto tab = ptr.get<t_spitetab>())
            spitetab_float(tab.get(), index);
    }

    void paintCells(Graphics& g)
    {
        auto const area = getLocalBounds().toFloat();
        auto const radius = static_cast<float>(getValue<int>(roundness));
        auto const font = Fonts::getCurrentFont().withHeight(getValue<int>(fontSize));
        g.setFont(font);

        for (int i = 0; i < itemTexts.size(); i++) {
            auto const cell = getCellBounds(i, area);
            auto const on = i == selected;
            auto const bg = Colour::fromString((on ? selectedBackgroundColour : backgroundColour).toString());
            auto const fg = Colour::fromString((on ? selectedForegroundColour : foregroundColour).toString());

            g.setColour(bg);
            g.fillRoundedRectangle(cell, std::min(radius, std::min(cell.getWidth(), cell.getHeight()) / 2.0f));

            if (auto* drawable = drawables[i]) {
                auto const picture = cell.reduced(std::max(1.0f, cell.getHeight() * 0.12f));
                if (drawableIsVector[i]) {
                    // live.tab's remapsvgcolors: the picture takes the cell's text colour, through its alpha
                    auto const scale = getImageScale();
                    Image mask(Image::ARGB, std::max(1, roundToInt(picture.getWidth() * scale)), std::max(1, roundToInt(picture.getHeight() * scale)), true);
                    {
                        Graphics mg(mask);
                        drawable->drawWithin(mg, mask.getBounds().toFloat(), RectanglePlacement::centred, 1.0f);
                    }
                    g.setColour(fg);
                    g.drawImage(mask, picture, RectanglePlacement::centred, true);
                } else {
                    drawable->drawWithin(g, picture, RectanglePlacement::centred, 1.0f);
                }
            } else {
                g.setColour(fg);
                g.drawFittedText(itemTexts[i], cell.toNearestInt().reduced(2, 0), Justification::centred, 1, 0.8f);
            }
        }
    }

    void render(NVGcontext* nvg) override
    {
        auto const scale = getImageScale();
        auto const w = roundToInt(getWidth() * scale);
        auto const h = roundToInt(getHeight() * scale);
        if (w <= 0 || h <= 0)
            return;

        if (needsRedraw || image.needsUpdate(w, h)) {
            image = NVGImage(nvg, w, h, [this, scale](Graphics& g) {
                g.addTransform(AffineTransform::scale(scale));
                paintCells(g);
            });
            needsRedraw = false;
        }

        NVGScopedState state(nvg);
        nvgScale(nvg, 1.0f / scale, 1.0f / scale);
        image.render(nvg, { 0, 0, w, h }, true);

        if (object->isSelected() && !cnv->isGraph) {
            nvgScale(nvg, scale, scale);
            nvgDrawRoundedRect(nvg, 0, 0, getWidth(), getHeight(), nvgRGBA(0, 0, 0, 0), nvgColour(PlugDataColours::objectSelectedOutlineColour), Corners::objectCornerRadius);
        }
    }

    void resized() override
    {
        needsRedraw = true;
    }

    void receiveObjectMessage(hash32 const symbol, SmallArray<pd::Atom> const& atoms) override
    {
        switch (symbol) {
        case hash("float"):
        case hash("set"):
        case hash("bang"): {
            if (auto tab = ptr.get<t_spitetab>())
                selected = tab->x_sel;
            needsRedraw = true;
            repaint();
            break;
        }
        case hash("items"):
        case hash("pictures"):
        case hash("grid"):
        case hash("spacing"):
        case hash("round"):
        case hash("fontsize"):
        case hash("colors"):
        case hash("dim"):
        case hash("send"):
        case hash("receive"): {
            update();
            if (symbol == hash("dim"))
                object->updateBounds();
            break;
        }
        default:
            break;
        }
    }

    void sendWords(char const* method, String const& text)
    {
        SmallArray<t_atom> atoms;
        for (auto const& word : splitWords(text)) {
            atoms.emplace_back();
            SETSYMBOL(&atoms.back(), pd->generateSymbol(word));
        }
        if (auto tab = ptr.get<t_pd>())
            pd_typedmess(tab.get(), pd->generateSymbol(method), atoms.size(), atoms.data());
    }

    void propertyChanged(Value& v) override
    {
        if (v.refersToSameSourceAs(sizeProperty)) {
            auto const& arr = *sizeProperty.getValue().getArray();
            if (auto tab = ptr.get<t_spitetab>()) {
                tab->x_w = std::max(static_cast<int>(arr[0]), 8);
                tab->x_h = std::max(static_cast<int>(arr[1]), 8);
            }
            object->updateBounds();
        } else if (v.refersToSameSourceAs(items)) {
            sendWords("items", items.toString());
        } else if (v.refersToSameSourceAs(pictures)) {
            sendWords("pictures", pictures.toString());
        } else if (v.refersToSameSourceAs(sendSymbol)) {
            sendWords("send", sendSymbol.toString().replace(" ", "\\ "));
        } else if (v.refersToSameSourceAs(receiveSymbol)) {
            sendWords("receive", receiveSymbol.toString().replace(" ", "\\ "));
            object->updateIolets();
        } else {
            if (auto tab = ptr.get<t_spitetab>()) {
                if (v.refersToSameSourceAs(rows))
                    tab->x_rows = std::max(getValue<int>(rows), 0);
                else if (v.refersToSameSourceAs(columns))
                    tab->x_cols = std::max(getValue<int>(columns), 0);
                else if (v.refersToSameSourceAs(initial))
                    tab->x_init = std::max(getValue<int>(initial), 0);
                else if (v.refersToSameSourceAs(loadInit))
                    tab->x_loadinit = getValue<bool>(loadInit);
                else if (v.refersToSameSourceAs(spacing))
                    tab->x_spacing = std::max(getValue<int>(spacing), 0);
                else if (v.refersToSameSourceAs(roundness))
                    tab->x_round = std::max(getValue<int>(roundness), 0);
                else if (v.refersToSameSourceAs(fontSize))
                    tab->x_fontsize = std::max(getValue<int>(fontSize), 4);
                else if (v.refersToSameSourceAs(backgroundColour))
                    tab->x_bg = pd->generateSymbol("#" + Colour::fromString(backgroundColour.toString()).toDisplayString(false).toLowerCase());
                else if (v.refersToSameSourceAs(foregroundColour))
                    tab->x_fg = pd->generateSymbol("#" + Colour::fromString(foregroundColour.toString()).toDisplayString(false).toLowerCase());
                else if (v.refersToSameSourceAs(selectedBackgroundColour))
                    tab->x_onbg = pd->generateSymbol("#" + Colour::fromString(selectedBackgroundColour.toString()).toDisplayString(false).toLowerCase());
                else if (v.refersToSameSourceAs(selectedForegroundColour))
                    tab->x_onfg = pd->generateSymbol("#" + Colour::fromString(selectedForegroundColour.toString()).toDisplayString(false).toLowerCase());
            }
        }
        needsRedraw = true;
        repaint();
    }
};
