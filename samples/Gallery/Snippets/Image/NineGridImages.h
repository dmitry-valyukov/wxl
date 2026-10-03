auto example = StackPanel {
    TextBlock {u"The normal image"},
    Image {height = 82, source = u"Assets/SampleMedia/ninegrid.gif"},
    TextBlock {u"Image stretched evenly"},
    Image {height = 164, nineGrid = Thickness {3, 3, 3, 3}, source = u"Assets/SampleMedia/ninegrid.gif"},
    TextBlock {u"Image stretched using nine grid"},
    Image {height = 164, nineGrid = Thickness {30, 20, 30, 20}, source = u"Assets/SampleMedia/ninegrid.gif"},
};