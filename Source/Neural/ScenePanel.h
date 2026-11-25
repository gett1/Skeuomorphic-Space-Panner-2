#pragma once

#include "../PluginProcessor.h"

class ScenePanel final : public juce::Component
{
public:
    explicit ScenePanel(GettoneSpacePannerProcessor& p) : processor(p)
    {
        for (auto* component : std::array<juce::Component*,10>{ &group, &newGroup, &analyze, &targets, &role,
                                                                &lockTrack, &blend, &apply, &undo, &share })
            addAndMakeVisible(*component);
        group.setTextToShowWhenEmpty("Paste a session code", juce::Colours::grey);
        group.onReturnKey = [this] { join(); };
        group.onFocusLost = [this] { if (group.getText().trim() != processor.getSpatialGroup()) join(); };
        newGroup.onClick = [this]
        {
            group.setText(juce::Uuid().toString().substring(0, 12), false);
            join();
        };
        analyze.onClick = [this]
        {
            join();
            panner::SpatialNetwork::analyze(tracks);
            repaint();
        };
        targets.addItem("All tracks", 1);
        targets.setSelectedId(1);
        targets.onChange = [this] { repaint(); };
        role.addItem("Auto instrument", 1);
        for (int i = 0; i < static_cast<int>(panner::roles.size()); ++i)
            role.addItem(panner::roles[static_cast<size_t>(i)], i + 2);
        role.onChange = [this]
        {
            std::lock_guard lock(processor.spatialTrack->mutex);
            processor.spatialTrack->roleOverride = role.getSelectedId() - 2;
        };
        lockTrack.onClick = [this]
        {
            std::lock_guard lock(processor.spatialTrack->mutex);
            processor.spatialTrack->locked = lockTrack.getToggleState();
        };
        blend.setSliderStyle(juce::Slider::LinearHorizontal);
        blend.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
        blend.setRange(0.0, 100.0, 1.0);
        blend.setValue(100.0);
        blend.setTextValueSuffix("%");
        blend.setTitle("AI Amount");
        apply.onClick = [this] { panner::SpatialNetwork::apply(selection(), static_cast<float>(blend.getValue() / 100), false); repaint(); };
        undo.onClick = [this] { panner::SpatialNetwork::apply(selection(), 0.0f, true); repaint(); };
        share.onClick = [this] { shareRoom(); };
        refresh();
    }

    void refresh()
    {
        const auto code = processor.getSpatialGroup();
        if (!group.hasKeyboardFocus(false) && group.getText() != code)
            group.setText(code, false);
        auto current = panner::SpatialNetwork::instance().peers(code);
        if (current != tracks)
        {
            tracks = std::move(current);
            targets.clear(juce::dontSendNotification);
            targets.addItem("All tracks", 1);
            for (size_t i = 0; i < tracks.size(); ++i)
                targets.addItem(nameFor(*tracks[i], i), static_cast<int>(i) + 2);
            targets.setSelectedId(1, juce::dontSendNotification);
        }
        {
            std::lock_guard lock(processor.spatialTrack->mutex);
            role.setSelectedId(processor.spatialTrack->roleOverride + 2, juce::dontSendNotification);
            lockTrack.setToggleState(processor.spatialTrack->locked, juce::dontSendNotification);
        }
        repaint();
    }

    void setTheme(juce::Colour ink, juce::Colour panel)
    {
        if (ink == accent && panel == background)
            return;
        accent = ink;
        background = panel;
        for (auto* button : { &newGroup, &analyze, &apply, &undo, &share })
        {
            button->setColour(juce::TextButton::buttonColourId, accent.withAlpha(.28f).overlaidWith(juce::Colours::transparentBlack));
            button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        blend.setColour(juce::Slider::trackColourId, accent.withAlpha(.45f));
        blend.setColour(juce::Slider::thumbColourId, accent);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10);
        r.removeFromTop(27);
        auto top = r.removeFromTop(25);
        group.setBounds(top.removeFromLeft(top.getWidth() * 43 / 100).reduced(2, 0));
        newGroup.setBounds(top.removeFromLeft(top.getWidth() / 2).reduced(2, 0));
        analyze.setBounds(top.reduced(2, 0));
        auto tools = r.removeFromBottom(27);
        targets.setBounds(tools.removeFromLeft(tools.getWidth() * 38 / 100).reduced(2, 0));
        apply.setBounds(tools.removeFromLeft(tools.getWidth() * 2 / 5).reduced(2, 0));
        undo.setBounds(tools.removeFromLeft(tools.getWidth() * 2 / 5).reduced(2, 0));
        share.setBounds(tools.reduced(2, 0));
        r.removeFromBottom(3);
        auto options = r.removeFromBottom(24);
        role.setBounds(options.removeFromLeft(options.getWidth() * 39 / 100).reduced(2, 0));
        lockTrack.setBounds(options.removeFromLeft(options.getWidth() * 43 / 100).reduced(2, 0));
        blend.setBounds(options.reduced(2, 0));
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(background.darker(.2f));
        g.fillRoundedRectangle(bounds, 7);
        g.setColour(accent.withAlpha(.35f));
        g.drawRoundedRectangle(bounds.reduced(.5f), 7, 1);
        g.setColour(accent);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText("GLOBAL MIX", 13, 6, 150, 25, juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(tracks.size()) + " connected", getWidth()-155, 6, 142, 25, juce::Justification::centredRight);
        auto area = bounds.reduced(22);
        area.removeFromTop(40);
        area.removeFromBottom(46);
        g.setColour(accent.withAlpha(.14f));
        for (int i = 0; i < 5; ++i)
        {
            const auto x = area.getX() + area.getWidth() * i / 4.0f;
            const auto y = area.getY() + area.getHeight() * i / 4.0f;
            g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
            g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
        }
        for (float scale : { .28f, .57f, .86f })
            g.drawEllipse(area.withSizeKeepingCentre(area.getWidth() * scale, area.getHeight() * scale), 1);
        if (tracks.empty())
        {
            g.setColour(juce::Colours::white.withAlpha(.78f));
            g.drawFittedText("Create a session code, then paste it on each track.\nPlay your mix and press Analyze.", area.toNearestInt(), juce::Justification::centred, 2);
        }
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            const auto& track = *tracks[i];
            std::lock_guard lock(track.mutex);
            if (!track.parameters)
                continue;
            const auto at = [&area](float x, float z)
            {
                return juce::Point<float>{ juce::jmap(x, -1.0f, 1.0f, area.getX(), area.getRight()),
                                           juce::jmap(z, area.getBottom()-15, area.getY()+8) };
            };
            const auto point = at(track.parameters->getRawParameterValue("posX")->load(), track.parameters->getRawParameterValue("posZ")->load());
            const auto tint = accent.withRotatedHue(static_cast<float>(i % 7) * .11f);
            if (track.hasSuggestion && !track.locked)
            {
                const auto proposed = at(track.suggestion[0], track.suggestion[2]);
                g.setColour(tint.withAlpha(.55f));
                g.drawLine({ point, proposed }, 1);
                g.drawEllipse(juce::Rectangle<float>(13,13).withCentre(proposed), 1.5f);
            }
            g.setColour(tint.withAlpha(.2f));
            g.fillEllipse(juce::Rectangle<float>(26,26).withCentre(point));
            g.setColour(tint);
            g.fillEllipse(juce::Rectangle<float>(11,11).withCentre(point));
            auto title = track.name.isNotEmpty() ? track.name : "Track " + juce::String(static_cast<int>(i+1));
            if (track.roleOverride >= 0)
                title = panner::roles[static_cast<size_t>(track.roleOverride)];
            else if (auto frame = track.analysis.read(); frame[0] > 0.0f)
            {
                const auto scores = panner::neural::instrument.infer(frame, true);
                const auto best = std::max_element(scores.begin(), scores.end());
                if (*best >= .55f)
                    title = juce::String(panner::roles[static_cast<size_t>(best - scores.begin())])
                            + " " + juce::String(juce::roundToInt(*best * 100)) + "%";
            }
            g.drawFittedText(title + (track.locked ? " [locked]" : ""),
                             juce::Rectangle<float>(106,16).withCentre(point.translated(0,16)).toNearestInt(),
                             juce::Justification::centred, 1);
        }
    }

private:
    void join()
    {
        processor.setSpatialGroup(group.getText());
        refresh();
    }

    auto selection() const -> std::vector<std::shared_ptr<panner::SpatialTrack>>
    {
        const auto index = targets.getSelectedId() - 2;
        if (index >= 0 && static_cast<size_t>(index) < tracks.size())
            return { tracks[static_cast<size_t>(index)] };
        return tracks;
    }

    static juce::String nameFor(const panner::SpatialTrack& track, size_t i)
    {
        std::lock_guard lock(track.mutex);
        return track.name.isNotEmpty() ? track.name : "Track " + juce::String(static_cast<int>(i + 1));
    }

    void shareRoom()
    {
        constexpr std::array names{ "revSize", "absorb", "diffusion", "roomMode" };
        for (const auto& track : selection())
        {
            std::lock_guard lock(track->mutex);
            if (!track->parameters || track->locked)
                continue;
            for (auto name : names)
            {
                auto* parameter = track->parameters->getParameter(name);
                const auto target = juce::String(name) == "roomMode" ? 1.0f
                    : processor.treeState.getParameter(name)->getValue();
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(target);
                parameter->endChangeGesture();
            }
        }
    }

    GettoneSpacePannerProcessor& processor;
    std::vector<std::shared_ptr<panner::SpatialTrack>> tracks;
    juce::Colour accent, background;
    juce::TextEditor group;
    juce::TextButton newGroup{ "NEW CODE" }, analyze{ "ANALYZE MIX" }, apply{ "APPLY" }, undo{ "UNDO" }, share{ "SHARE ROOM" };
    juce::ComboBox targets, role;
    juce::ToggleButton lockTrack{ "Lock track" };
    juce::Slider blend;
};
