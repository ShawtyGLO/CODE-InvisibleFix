// ============================================================================
//  UILayout.h - GENERE depuis tools/layout.json par tools/generate.ps1
//  NE PAS EDITER A LA MAIN. Portee : panneaux peints (drawAdvPanel).
// ============================================================================
#pragma once

namespace UI
{
    struct Panel      { float x, y, w, h; const char* title; };
    struct Rect       { float x, y, w, h; };
    struct AnchorBR   { float right, bottom, w, h; }; // ancre bas-droite : x=getWidth()-right, y=getHeight()-bottom
    struct AnchorBottomBar { float bottom, h, w; };   // barre ancree en bas a gauche : y=getHeight()-bottom, largeur w (coords de base)
    // Texte peint par paint() : libelle statique pilote par les donnees.
    // align : 0 = gauche, 1 = centre, 2 = droite. La couleur reste choisie dans le C++ (Theme::),
    // seul son alpha vient des donnees.
    struct PText      { float x, y, w, h; const char* text; float size; bool bold; int align; float alpha; };

    namespace panel
    {
        // Onglet cleanAdv
        constexpr Panel nr            {   24.0f,  309.0f,  344.0f,  184.0f, "NOISE REDUCTION" };
        constexpr Panel gate          {  380.0f,  309.0f,  363.0f,  184.0f, "GATE" };
        // Onglet roomAdv
        constexpr Panel roomAnalysis  {   24.0f,  309.0f,  330.0f,  152.0f, "NOTCH ANALYSIS" };
        constexpr Panel roomParams    {  364.0f,  309.0f,  372.0f,  152.0f, "NOTCH PARAMETERS" };
        // Onglet deEssAdv
        constexpr Panel deEss         {   24.0f,  309.0f,  712.0f,  135.0f, "DE-ESSER ADVANCED" };
        // Onglet deRevAdv
        constexpr Panel deRev         {   24.0f,  309.0f,  712.0f,  152.0f, "DE-REV ADVANCED" };
        // Onglet airAdv
        constexpr Panel air           {   24.0f,  309.0f,  712.0f,  152.0f, "AIR CHARACTER" };
    }

    namespace ctrl
    {
        namespace facade
        {
            constexpr Rect cleanSlider            {   45.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect cleanLabel             {   22.0f,  243.0f,  136.0f,   14.0f };
            constexpr Rect roomSlider             {  189.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect roomLabel              {  166.0f,  243.0f,  136.0f,   14.0f };
            constexpr Rect reverbSlider           {  333.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect reverbLabel            {  312.0f,  242.0f,  136.0f,   14.0f };
            constexpr Rect deEssSlider            {  477.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect deEssLabel             {  456.0f,  242.0f,  136.0f,   14.0f };
            constexpr Rect airSlider              {  621.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect airLabel               {  600.0f,  242.0f,  136.0f,   14.0f };
            constexpr Rect cleanBypassBtn         {   32.0f,   97.0f,   26.0f,   14.0f };
            constexpr Rect roomBypassBtn          {  176.0f,   97.0f,   26.0f,   14.0f };
            constexpr Rect reverbBypassBtn        {  320.0f,   97.0f,   26.0f,   14.0f };
            constexpr Rect deEssBypassBtn         {  464.0f,   97.0f,   26.0f,   14.0f };
            constexpr Rect airBypassBtn           {  608.0f,   97.0f,   26.0f,   14.0f };
            constexpr Rect deltaBtn               {  136.0f,   97.0f,   16.0f,   14.0f };
            constexpr Rect roomDeltaBtn           {  280.0f,   97.0f,   16.0f,   14.0f };
            constexpr Rect reverbDeltaBtn         {  424.0f,   97.0f,   16.0f,   14.0f };
            constexpr Rect deEssDeltaBtn          {  568.0f,   97.0f,   16.0f,   14.0f };
            constexpr Rect airDeltaBtn            {  712.0f,   97.0f,   16.0f,   14.0f };
            constexpr Rect masterGainLabel        {  580.0f,   42.0f,   80.0f,   16.0f };
            constexpr Rect masterGainSlider       {  666.0f,   36.0f,   28.0f,   28.0f };
            constexpr Rect globalBypassBtn        {   22.0f,   40.0f,  160.0f,   28.0f };
            constexpr Rect browsePresetBtn        {    4.0f,    4.0f,   24.0f,   20.0f };
            constexpr Rect prevPresetBtn          {  202.0f,    2.0f,   24.0f,   24.0f };
            constexpr Rect presetNameLabel        {  228.0f,    0.0f,  302.0f,   28.0f };
            constexpr Rect nextPresetBtn          {  532.0f,    2.0f,   24.0f,   24.0f };
            constexpr Rect savePresetBtn          {  558.0f,    5.0f,   42.0f,   18.0f };
            constexpr Rect initPresetBtn          {  602.0f,    5.0f,   40.0f,   18.0f };
            constexpr Rect liveHQBtn              {  644.0f,    4.0f,   68.0f,   20.0f };
        }
        namespace cleanAdv
        {
            constexpr Rect nrBandDisplay          {   31.0f,  336.0f,  281.0f,  114.0f };
            constexpr Rect nrBypassBtn            {  124.0f,  320.0f,   60.0f,   13.0f };
            constexpr Rect nrRatioSlider          {   31.0f,  452.0f,  236.0f,   14.0f };
            constexpr Rect nrSmoothSlider         {   31.0f,  472.0f,  236.0f,   14.0f };
            constexpr Rect nrArtifactBlendSlider  {  322.0f,  446.0f,   35.0f,   40.0f };
            constexpr Rect nrLearnBtn             {  303.0f,  320.0f,   54.0f,   13.0f };
            constexpr Rect gateBypassBtn          {  416.0f,  320.0f,   66.0f,   13.0f };
            constexpr Rect gateWaveform           {  390.0f,  336.0f,  260.0f,  114.0f };
            constexpr Rect gateAHR                {  390.0f,  452.0f,  160.0f,   34.0f };
            constexpr Rect plosiveProtectSlider   {  675.0f,  450.0f,   44.0f,   36.0f };
            constexpr Rect gateAutoBtn            {  682.0f,  320.0f,   54.0f,   13.0f };
            constexpr Rect nrRatioLabel           {  267.0f,  452.0f,   38.0f,   14.0f };
            constexpr Rect nrSmoothLabel          {  267.0f,  472.0f,   38.0f,   14.0f };
            constexpr Rect nrArtifactBlendLabel   {  319.0f,  435.0f,   42.0f,   11.0f };
            constexpr Rect plosiveLabel           {  663.0f,  435.0f,   68.0f,   11.0f };
        }
        namespace roomAdv
        {
            constexpr Rect roomVisualizer         {   33.0f,  301.0f,  312.0f,  121.0f };
            constexpr Rect advRoomFreqSlider      {  418.0f,  301.0f,   50.0f,   56.0f };
            constexpr Rect advRoomQSlider         {  510.0f,  301.0f,   50.0f,   56.0f };
            constexpr Rect advRoomDepthBoostSlider {  600.0f,  301.0f,   50.0f,   56.0f };
            constexpr Rect advRoomFreqLabel       {  416.0f,  357.0f,   54.0f,   10.0f };
            constexpr Rect advRoomQLabel          {  508.0f,  357.0f,   54.0f,   10.0f };
            constexpr Rect advRoomDepthBoostLabel {  598.0f,  357.0f,   54.0f,   10.0f };
            constexpr Rect roomTrackerChip        {  374.0f,  397.0f,  352.0f,   56.0f };
        }
        namespace deEssAdv
        {
            constexpr Rect deEssDetect            {   34.0f,  304.0f,  360.0f,   84.0f };
            constexpr Rect advDeEssDetectSlider   {   34.0f,  393.0f,  318.0f,   16.0f };
            constexpr Rect deEssAR                {  403.0f,  304.0f,  218.0f,   60.0f };
            constexpr Rect advDeEssIntenSlider    {  663.0f,  305.0f,   30.0f,   88.0f };
            constexpr Rect advDeEssDetectLabel    {  362.0f,  393.0f,   18.0f,   16.0f };
            constexpr Rect advDeEssIntenLabel     {  651.0f,  393.0f,   54.0f,   10.0f };
        }
        namespace deRevAdv
        {
            constexpr Rect deRevCanvas            {   36.0f,  305.0f,  606.0f,  120.0f };
            constexpr Rect deRevFloorSlider       {  657.0f,  305.0f,   50.0f,   66.0f };
            constexpr Rect deRevLinkBtn           {  657.0f,  397.0f,   52.0f,   18.0f };
            constexpr Rect deRevFloorLabel        {  655.0f,  366.0f,   54.0f,   10.0f };
        }
        namespace airAdv
        {
            constexpr Rect advAirFreqSlider       {   38.0f,  406.0f,  414.0f,   20.0f };
            constexpr Rect advAirDriveSlider      {  464.0f,  294.0f,   50.0f,   66.0f };
            constexpr Rect airSatTriodeBtn        {  520.0f,  302.0f,   98.0f,   19.0f };
            constexpr Rect airSatClassABtn        {  622.0f,  302.0f,   98.0f,   19.0f };
            constexpr Rect airSatTapeBtn          {  520.0f,  327.0f,   98.0f,   19.0f };
            constexpr Rect airSatIronBtn          {  622.0f,  327.0f,   98.0f,   19.0f };
            constexpr Rect advAirShapeLabel       {  464.0f,  374.0f,  264.0f,   11.0f };
            constexpr Rect advAirShapeSlider      {  468.0f,  385.0f,  256.0f,   20.0f };
            constexpr Rect advAirDriveLabel       {  460.0f,  350.0f,   58.0f,   10.0f };
        }
    }

    namespace ptext
    {
        namespace cleanAdv
        {
            constexpr PText nrActivityCaption      {  190.0f,  321.0f,  110.0f,    9.0f, "NR ACTIVITY", 7.0f, true , 1, 0.6f };
        }
        namespace roomAdv
        {
            constexpr PText notchFreqCaption       {  380.0f,  335.0f,   80.0f,   10.0f, "NOTCH FREQ", 8.0f, true , 0, 1.0f };
            constexpr PText notchQCaption          {  476.0f,  335.0f,   60.0f,   10.0f, "NOTCH Q", 8.0f, true , 0, 1.0f };
        }
        namespace airAdv
        {
            constexpr PText airHeatCaption         {  464.0f,  311.0f,   58.0f,   10.0f, "HEAT", 7.5f, true , 1, 0.8f };
            constexpr PText airSatTypeCaption      {  520.0f,  311.0f,  200.0f,   10.0f, "SAT TYPE", 7.5f, true , 1, 0.8f };
            constexpr PText airCharacterCaption    {   38.0f,  456.0f,  200.0f,   10.0f, "CHARACTER", 7.0f, false, 0, 0.6f };
        }
    }

    namespace stateHeight
    {
        constexpr float facade     =  338.0f;
        constexpr float cleanAdv   =  525.0f;
        constexpr float roomAdv    =  493.0f;
        constexpr float deEssAdv   =  493.0f;
        constexpr float deRevAdv   =  493.0f;
        constexpr float airAdv     =  493.0f;
    }

    namespace anchor
    {
        namespace br
        {
            constexpr AnchorBR abBtnA           {  212.0f,   21.0f,   24.0f,   14.0f };
            constexpr AnchorBR abArrowBtn       {  186.0f,   21.0f,   36.0f,   14.0f };
            constexpr AnchorBR abBtnB           {  148.0f,   21.0f,   24.0f,   14.0f };
            constexpr AnchorBR undoBtn          {  122.0f,   21.0f,   28.0f,   14.0f };
            constexpr AnchorBR redoBtn          {   92.0f,   21.0f,   28.0f,   14.0f };
            constexpr AnchorBR scaleBtn         {   62.0f,   21.0f,   58.0f,   14.0f };
        }
        namespace bottomBar
        {
            constexpr AnchorBottomBar tooltipBar       {   28.0f,   28.0f,  524.0f };
        }
    }
}
