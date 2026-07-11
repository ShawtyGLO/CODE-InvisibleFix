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
    struct AnchorBottomBar { float bottom, h; };      // barre pleine largeur ancree en bas : y=getHeight()-bottom

    namespace panel
    {
        // Onglet cleanAdv
        constexpr Panel nr            {   24.0f,  281.0f,  296.0f,  155.0f, "NOISE REDUCTION" };
        constexpr Panel gate          {  334.0f,  281.0f,  402.0f,  155.0f, "GATE" };
        // Onglet roomAdv
        constexpr Panel roomAnalysis  {   24.0f,  281.0f,  330.0f,  152.0f, "NOTCH ANALYSIS" };
        constexpr Panel roomParams    {  364.0f,  281.0f,  372.0f,  152.0f, "NOTCH PARAMETERS" };
        // Onglet deEssAdv
        constexpr Panel deEss         {   24.0f,  281.0f,  712.0f,  130.0f, "DE-ESSER ADVANCED" };
        // Onglet deRevAdv
        constexpr Panel deRev         {   24.0f,  281.0f,  712.0f,  152.0f, "DE-REV ADVANCED" };
        // Onglet airAdv
        constexpr Panel air           {   24.0f,  281.0f,  712.0f,  152.0f, "AIR CHARACTER" };
    }

    namespace ctrl
    {
        namespace facade
        {
            constexpr Rect cleanSlider            {   45.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect cleanLabel             {   24.0f,  242.0f,  136.0f,   14.0f };
            constexpr Rect roomSlider             {  189.0f,  123.0f,   94.0f,  114.0f };
            constexpr Rect roomLabel              {  168.0f,  242.0f,  136.0f,   14.0f };
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
            constexpr Rect nrBandDisplay          {   34.0f,  321.0f,  266.0f,  116.0f };
            constexpr Rect nrBypassBtn            {   34.0f,  314.0f,   60.0f,   13.0f };
            constexpr Rect nrRatioSlider          {   34.0f,  445.0f,  260.0f,   14.0f };
            constexpr Rect nrSmoothSlider         {   34.0f,  463.0f,  260.0f,   14.0f };
            constexpr Rect nrArtifactBlendSlider  {  304.0f,  346.0f,   34.0f,   50.0f };
            constexpr Rect nrLearnBtn             {  248.0f,  314.0f,   54.0f,   13.0f };
            constexpr Rect gateBypassBtn          {  342.0f,  314.0f,   66.0f,   13.0f };
            constexpr Rect gateWaveform           {  342.0f,  326.0f,  186.0f,  130.0f };
            constexpr Rect gateAHR                {  534.0f,  326.0f,  192.0f,   60.0f };
            constexpr Rect plosiveProtectSlider   {  668.0f,  462.0f,   44.0f,   36.0f };
            constexpr Rect gateAutoBtn            {  412.0f,  314.0f,   54.0f,   13.0f };
            constexpr Rect nrRatioLabel           {  296.0f,  445.0f,   38.0f,   14.0f };
            constexpr Rect nrSmoothLabel          {  296.0f,  463.0f,   38.0f,   14.0f };
            constexpr Rect nrArtifactBlendLabel   {  300.0f,  398.0f,   42.0f,   11.0f };
            constexpr Rect plosiveLabel           {  656.0f,  500.0f,   68.0f,   11.0f };
        }
        namespace roomAdv
        {
            constexpr Rect roomVisualizer         {   58.0f,  337.0f,  296.0f,  112.0f };
            constexpr Rect advRoomFreqSlider      {  380.0f,  347.0f,   50.0f,   66.0f };
            constexpr Rect advRoomQSlider         {  476.0f,  347.0f,   50.0f,   66.0f };
            constexpr Rect advRoomDepthBoostSlider {  572.0f,  347.0f,   50.0f,   66.0f };
            constexpr Rect advRoomFreqLabel       {  378.0f,  415.0f,   54.0f,   10.0f };
            constexpr Rect advRoomQLabel          {  474.0f,  415.0f,   54.0f,   10.0f };
            constexpr Rect advRoomDepthBoostLabel {  570.0f,  415.0f,   54.0f,   10.0f };
        }
        namespace deEssAdv
        {
            constexpr Rect deEssDetect            {   34.0f,  317.0f,  360.0f,   84.0f };
            constexpr Rect advDeEssDetectSlider   {   34.0f,  404.0f,  318.0f,   16.0f };
            constexpr Rect deEssAR                {  402.0f,  317.0f,  218.0f,   60.0f };
            constexpr Rect advDeEssIntenSlider    {  630.0f,  321.0f,   30.0f,   88.0f };
            constexpr Rect advDeEssDetectLabel    {  354.0f,  404.0f,   18.0f,   16.0f };
            constexpr Rect advDeEssIntenLabel     {  618.0f,  411.0f,   54.0f,   10.0f };
        }
        namespace deRevAdv
        {
            constexpr Rect deRevCanvas            {   36.0f,  305.0f,  606.0f,  120.0f };
            constexpr Rect deRevFloorSlider       {  656.0f,  299.0f,   50.0f,   66.0f };
            constexpr Rect deRevLinkBtn           {  656.0f,  387.0f,   52.0f,   18.0f };
            constexpr Rect deRevFloorLabel        {  654.0f,  367.0f,   54.0f,   10.0f };
        }
        namespace airAdv
        {
            constexpr Rect advAirFreqSlider       {   38.0f,  406.0f,  414.0f,   20.0f };
            constexpr Rect advAirDriveSlider      {  464.0f,  294.0f,   50.0f,   66.0f };
            constexpr Rect airSatTriodeBtn        {  520.0f,  292.0f,   98.0f,   19.0f };
            constexpr Rect airSatClassABtn        {  622.0f,  292.0f,   98.0f,   19.0f };
            constexpr Rect airSatTapeBtn          {  520.0f,  315.0f,   98.0f,   19.0f };
            constexpr Rect airSatIronBtn          {  622.0f,  315.0f,   98.0f,   19.0f };
            constexpr Rect advAirShapeLabel       {  464.0f,  372.0f,  264.0f,   11.0f };
            constexpr Rect advAirShapeSlider      {  468.0f,  385.0f,  256.0f,   20.0f };
            constexpr Rect advAirDriveLabel       {  460.0f,  362.0f,   58.0f,   10.0f };
        }
    }

    namespace anchor
    {
        namespace br
        {
            constexpr AnchorBR abBtnA           {  212.0f,   18.0f,   24.0f,   14.0f };
            constexpr AnchorBR abArrowBtn       {  186.0f,   18.0f,   36.0f,   14.0f };
            constexpr AnchorBR abBtnB           {  148.0f,   18.0f,   24.0f,   14.0f };
            constexpr AnchorBR undoBtn          {  122.0f,   18.0f,   28.0f,   14.0f };
            constexpr AnchorBR redoBtn          {   92.0f,   18.0f,   28.0f,   14.0f };
            constexpr AnchorBR scaleBtn         {   62.0f,   18.0f,   58.0f,   14.0f };
        }
        namespace bottomBar
        {
            constexpr AnchorBottomBar tooltipBar       {   48.0f,   20.0f };
        }
    }
}
