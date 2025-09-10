#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual VariableRange<ScalableGenerator>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Simplex> : MetadataT<VariableRange<ScalableGenerator>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description = 
                "Smooth gradient noise from an N dimensional simplex grid\n"
                "Developed by Ken Perlin in 2001";
        }
    };
#endif

    class SuperSimplex : public virtual VariableRange<ScalableGenerator>
    {
    public:
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SuperSimplex> : MetadataT<VariableRange<ScalableGenerator>>
    {
        SmartNode<> CreateNode( FastSIMD::FeatureSet ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description =
                "Extra smooth gradient noise from an N dimensional simplex grid\n"
                "Slower to generate than Simplex noise\n"
                "Developed by K.jpg";
        }
    };
#endif
}
