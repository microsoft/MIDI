// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatchLayoutTests.h"

#include "PatchLayout.h"

#include <cmath>
#include <random>
#include <string>

using namespace midipatchbay;

namespace
{
    void AddEndpoint(PatchDocument& patch, std::wstring const& id, double y = 0)
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.CanvasY = y;
        patch.Endpoints.push_back(endpoint);
    }

    void AddBlock(PatchDocument& patch, std::wstring const& id)
    {
        PatchBlock block{};
        block.Id = id;
        block.Kind = BlockKind::Transpose;
        block.Settings = DefaultBlockSettings(block.Kind);
        patch.Blocks.push_back(block);
    }

    void Link(PatchDocument& patch, std::wstring const& from, std::wstring const& to)
    {
        PatchConnection link{};
        link.Id = from + L">" + to;
        link.SourceId = from;
        link.DestinationId = to;
        patch.Connections.push_back(link);
    }

    double X(PatchDocument& patch, std::wstring const& id)
    {
        return *patch.NodeX(id);
    }

    // Every pair of nodes, by the sizes the layout was given.
    bool AnythingOverlaps(PatchDocument& patch)
    {
        std::vector<std::wstring> ids{};

        for (auto const& endpoint : patch.Endpoints) ids.push_back(endpoint.Id);
        for (auto const& block : patch.Blocks) ids.push_back(block.Id);

        for (size_t a = 0; a < ids.size(); a++)
        {
            for (size_t b = a + 1; b < ids.size(); b++)
            {
                auto const sizeA = EstimatedNodeSize(patch, ids[a]);
                auto const sizeB = EstimatedNodeSize(patch, ids[b]);

                auto const ax = *patch.NodeX(ids[a]);
                auto const ay = *patch.NodeY(ids[a]);
                auto const bx = *patch.NodeX(ids[b]);
                auto const by = *patch.NodeY(ids[b]);

                auto const apart = ax + sizeA.Width <= bx || bx + sizeB.Width <= ax ||
                    ay + sizeA.Height <= by || by + sizeB.Height <= ay;

                if (!apart)
                {
                    return true;
                }
            }
        }

        return false;
    }
}

void PatchLayoutTests::MessagesFlowLeftToRight()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"keys");
    AddEndpoint(patch, L"pads", 300);
    AddEndpoint(patch, L"synth");
    AddEndpoint(patch, L"drums");
    AddBlock(patch, L"split");
    AddBlock(patch, L"up");
    AddBlock(patch, L"remap");

    Link(patch, L"keys", L"split");
    Link(patch, L"split", L"up");
    Link(patch, L"up", L"synth");
    Link(patch, L"pads", L"remap");
    Link(patch, L"remap", L"drums");

    ArrangeInColumns(patch, nullptr);

    VERIFY_IS_TRUE(X(patch, L"keys") < X(patch, L"split"));
    VERIFY_IS_TRUE(X(patch, L"split") < X(patch, L"up"));
    VERIFY_IS_TRUE(X(patch, L"up") < X(patch, L"synth"));
    VERIFY_IS_TRUE(X(patch, L"pads") < X(patch, L"remap"));

    // Everything that only receives lines up on the right.
    VERIFY_ARE_EQUAL(X(patch, L"synth"), X(patch, L"drums"));

    // The sources keep the order they had, top to bottom.
    VERIFY_IS_TRUE(*patch.NodeY(L"keys") < *patch.NodeY(L"pads"));

    VERIFY_IS_FALSE(AnythingOverlaps(patch));
}

void PatchLayoutTests::NothingLandsOnAnythingElse()
{
    std::mt19937 random{ 1256u };

    for (int round = 0; round < 50; round++)
    {
        PatchDocument patch{};

        auto const endpoints = std::uniform_int_distribution<int>(1, 8)(random);
        auto const blocks = std::uniform_int_distribution<int>(0, 20)(random);

        std::vector<std::wstring> ids{};

        for (int i = 0; i < endpoints; i++)
        {
            ids.push_back(L"e" + std::to_wstring(i));
            AddEndpoint(patch, ids.back(), std::uniform_int_distribution<int>(0, 2000)(random));

            if (i % 3 == 0)
            {
                patch.Endpoints.back().ShowAllGroups = true;
            }
        }

        for (int i = 0; i < blocks; i++)
        {
            ids.push_back(L"b" + std::to_wstring(i));
            AddBlock(patch, ids.back());
        }

        auto const links = std::uniform_int_distribution<int>(0, 40)(random);
        std::uniform_int_distribution<size_t> pick(0, ids.size() - 1);

        for (int i = 0; i < links; i++)
        {
            Link(patch, ids[pick(random)], ids[pick(random)]);
        }

        ArrangeInColumns(patch, nullptr);

        VERIFY_IS_FALSE(AnythingOverlaps(patch));

        for (auto const& id : ids)
        {
            VERIFY_IS_TRUE(std::isfinite(*patch.NodeX(id)));
            VERIFY_IS_TRUE(std::isfinite(*patch.NodeY(id)));
        }
    }
}

void PatchLayoutTests::ALoopStillGetsALayout()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");
    AddBlock(patch, L"a");
    AddBlock(patch, L"b");

    Link(patch, L"in", L"a");
    Link(patch, L"a", L"b");
    Link(patch, L"b", L"a");
    Link(patch, L"b", L"out");

    ArrangeInColumns(patch, nullptr);

    VERIFY_IS_TRUE(X(patch, L"in") < X(patch, L"a"));
    VERIFY_IS_TRUE(X(patch, L"a") < X(patch, L"b"));
    VERIFY_IS_TRUE(X(patch, L"b") < X(patch, L"out"));
    VERIFY_IS_FALSE(AnythingOverlaps(patch));
}

void PatchLayoutTests::AnAnnotationStaysWhereItWasPut()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"keys");
    AddEndpoint(patch, L"synth");
    AddBlock(patch, L"up");

    Link(patch, L"keys", L"up");
    Link(patch, L"up", L"synth");

    PatchBlock note{};
    note.Id = L"note";
    note.Kind = BlockKind::Annotation;
    note.Settings = DefaultBlockSettings(note.Kind);
    note.Settings.Annotation.Text = L"Up an octave for the pads";
    note.CanvasX = 1234;
    note.CanvasY = -56;
    patch.Blocks.push_back(note);

    ArrangeInColumns(patch, nullptr);

    VERIFY_ARE_EQUAL(1234.0, X(patch, L"note"));
    VERIFY_ARE_EQUAL(-56.0, *patch.NodeY(L"note"));
    VERIFY_IS_TRUE(X(patch, L"keys") < X(patch, L"up"));
    VERIFY_IS_TRUE(X(patch, L"up") < X(patch, L"synth"));

    // Its size follows its text, so a bigger font takes more room.
    auto const small = EstimatedNodeSize(patch, L"note");

    patch.Blocks.back().Settings.Annotation.FontSize = 32;

    auto const large = EstimatedNodeSize(patch, L"note");

    VERIFY_IS_TRUE(large.Width > small.Width);
    VERIFY_IS_TRUE(large.Height > small.Height);
}
