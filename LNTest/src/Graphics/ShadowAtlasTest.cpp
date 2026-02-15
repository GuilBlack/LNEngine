#include "Engine/Graphics/Utils/ShadowAtlas.h"

using namespace lne;

// used ChatGPT to generate the test cases then I tweaked it because I'm lazy for testing :)

bool IsInvalid(const ShadowAtlasChunk& c)
{
    return c.ID == INVALID_SHADOW_ATLAS_CHUNK.ID
        && c.Dimension == INVALID_SHADOW_ATLAS_CHUNK.Dimension
        && c.Offset == INVALID_SHADOW_ATLAS_CHUNK.Offset;
}

static inline void ExpectValidChunk(const ShadowAtlasChunk& c)
{
    EXPECT_NE(c.ID, INVALID_SHADOW_ATLAS_CHUNK.ID);
    EXPECT_NE(c.Dimension, INVALID_SHADOW_ATLAS_CHUNK.Dimension);
    EXPECT_NE(c.Offset, INVALID_SHADOW_ATLAS_CHUNK.Offset);
}

static inline void ExpectChunk(const ShadowAtlasChunk& c, u32 dim, glm::uvec2 off)
{
    ExpectValidChunk(c);
    EXPECT_EQ(c.Dimension, dim);
    EXPECT_EQ(c.Offset, off);
}

class ShadowAtlasFixture : public ::testing::Test {};

// -------------------------
// Constructor requirements
// -------------------------

TEST_F(ShadowAtlasFixture, Ctor_CeilsDimensionAndMinSizeToNextPowerOfTwo)
{
    // dimension=300 -> 512, min=3 -> 4
    ShadowAtlas atlas(300u, 3u);

    EXPECT_EQ(atlas.GetAtlasDimension(), 512u);
    EXPECT_EQ(atlas.GetMinSizeLimit(), 4u);

    ShadowAtlas atlas2(2048u, 512u);
    EXPECT_EQ(atlas2.GetAtlasDimension(), 2048u);
    EXPECT_EQ(atlas2.GetMinSizeLimit(), 512u);
}

TEST_F(ShadowAtlasFixture, Ctor_ClampsMinSizeToDimensionIfTooLarge)
{
    // dimension=64, min=128 -> min should clamp to 64 (after pow2 coercion)
    ShadowAtlas atlas(64u, 128u);

    EXPECT_EQ(atlas.GetAtlasDimension(), 64u);
    EXPECT_EQ(atlas.GetMinSizeLimit(), 64u);
}

// -------------------------
// Reserve sizing rules
// -------------------------

TEST_F(ShadowAtlasFixture, Reserve_CeilsRequestedSizeToNextPowerOfTwo)
{
    ShadowAtlas atlas(64u, 2u);

    // Request 3 -> should ceil to 4
    ShadowAtlasChunk c = atlas.Reserve(3u);
    ExpectValidChunk(c);
    EXPECT_EQ(c.Dimension, 4u);
}

TEST_F(ShadowAtlasFixture, Reserve_FailsIfRequestedBelowMinSizeLimit)
{
    ShadowAtlas atlas(64u, 8u);

    ShadowAtlasChunk c = atlas.Reserve(4u);
    EXPECT_TRUE(IsInvalid(c));
}

TEST_F(ShadowAtlasFixture, Reserve_FailsIfRequestedAboveAtlasDimension)
{
    ShadowAtlas atlas(64u, 2u);

    ShadowAtlasChunk c = atlas.Reserve(128u);
    EXPECT_TRUE(IsInvalid(c));
}

TEST_F(ShadowAtlasFixture, Reserve_FailsWhenAtlasIsFullForThatChunkSize)
{
    // 4x4 atlas with min=2 -> can fit exactly four 2x2 chunks
    ShadowAtlas atlas(/*dimension*/4u, /*minSizeLimit*/2u);

    ShadowAtlasChunk a = atlas.Reserve(2u);
    ShadowAtlasChunk b = atlas.Reserve(2u);
    ShadowAtlasChunk c = atlas.Reserve(2u);
    ShadowAtlasChunk d = atlas.Reserve(2u);

    ExpectValidChunk(a);
    ExpectValidChunk(b);
    ExpectValidChunk(c);
    ExpectValidChunk(d);

    ShadowAtlasChunk e = atlas.Reserve(2u);
    EXPECT_TRUE(IsInvalid(e));
}

TEST_F(ShadowAtlasFixture, Reserve_AllocationOrder_IsTopLeftTopRightBottomLeftBottomRight)
{
    // With an 8x8 atlas and min=2, the first four 2x2 reservations should fill the
    // top-left 4x4 region in child order TL, TR, BL, BR (within that 4x4).
    ShadowAtlas atlas(/*dimension*/8u, /*minSizeLimit*/2u);

    ShadowAtlasChunk c0 = atlas.Reserve(2u);
    ShadowAtlasChunk c1 = atlas.Reserve(2u);
    ShadowAtlasChunk c2 = atlas.Reserve(2u);
    ShadowAtlasChunk c3 = atlas.Reserve(2u);

    // Expected offsets inside the top-left 4x4 block:
    // TL: (0,0), TR: (2,0), BL: (0,2), BR: (2,2)
    ExpectChunk(c0, 2u, { 0u, 0u });
    ExpectChunk(c1, 2u, { 2u, 0u });
    ExpectChunk(c2, 2u, { 0u, 2u });
    ExpectChunk(c3, 2u, { 2u, 2u });
}

TEST_F(ShadowAtlasFixture, Release_InvalidId_DoesNotBreakFutureAllocations)
{
    ShadowAtlas atlas(8u, 2u);

    ShadowAtlasChunk a = atlas.Reserve(2u);
    ShadowAtlasChunk b = atlas.Reserve(2u);
    ExpectValidChunk(a);
    ExpectValidChunk(b);

    // Should be a no-op / safe:
    atlas.Release(1234567);

    // Releasing the real chunks should still work fine:
    atlas.Release(a.ID);
    atlas.Release(b.ID);

    // After freeing, we should be able to allocate again.
    ShadowAtlasChunk c = atlas.Reserve(4u);
    ExpectValidChunk(c);
    EXPECT_EQ(c.Dimension, 4u);
}

TEST_F(ShadowAtlasFixture, Release_MergesParents_WhenLastChildFreed_EnablingLargerAllocation)
{
    // Goal: prove that after releasing all 2x2 chunks under a 4x4 quadrant,
    // the parent nodes get removed (merged), allowing a 4x4 allocation there.
    //
    // IMPORTANT: To make this test meaningful, we must ensure the other three 4x4
    // quadrants are "fragmented" (have at least one 2x2 allocation), so a 4x4 chunk
    // cannot be placed anywhere else. Then reserve(4) should succeed ONLY if TL merges.
    ShadowAtlas atlas(8u, 2u);

    // Allocate 2x2 chunks:
    // - First 4 fill TL quadrant (top-left 4x4): offsets (0,0)(2,0)(0,2)(2,2)
    // - Next 4 fill TR quadrant (top-right 4x4): offsets (4,0)(6,0)(4,2)(6,2)
    // - Next 4 fill BL quadrant (bottom-left 4x4): offsets (0,4)(2,4)(0,6)(2,6)
    // - Next 1 allocates into BR quadrant, fragmenting it: offset (4,4)
    ShadowAtlasChunk chunks[13];
    for (int i = 0; i < 13; ++i)
    {
        chunks[i] = atlas.Reserve(2u);
        ASSERT_FALSE(IsInvalid(chunks[i])) << "Failed at i=" << i;
        ASSERT_EQ(chunks[i].Dimension, 2u);
    }

    // Release the 4 chunks in the TL quadrant (the first 4 allocations).
    atlas.Release(chunks[0].ID);
    atlas.Release(chunks[1].ID);
    atlas.Release(chunks[2].ID);
    atlas.Release(chunks[3].ID);

    // Now try to allocate a 4x4 chunk.
    // - If TL did NOT merge (children still exist in tree), reserve(4) should fail everywhere:
    //   TR/BL/BR are fragmented by 2x2 allocations.
    // - If TL DID merge, reserve(4) should succeed at offset (0,0) due to priority.
    ShadowAtlasChunk big = atlas.Reserve(4u);

    ASSERT_FALSE(IsInvalid(big)) << "Expected reserve(4) to succeed only if TL merged";
    EXPECT_EQ(big.Dimension, 4u);
    EXPECT_EQ(big.Offset, (glm::uvec2{ 0u, 0u }));
}
