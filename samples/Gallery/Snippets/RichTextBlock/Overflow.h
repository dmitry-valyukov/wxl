// The text flows from the RichTextBlock into the first container, and from it
// into the second: each names the next one.
auto second = RichTextBlockOverflow {column = 2, Margin {12, 0}};

auto first = RichTextBlockOverflow {
    column = 1,
    Margin {12, 0},
    overflowContentTarget = second,
};

auto text = RichTextBlock {
    column = 0,
    Margin {12, 0},
    textAlignment.justify,
    overflowContentTarget = first,
    Paragraph {
        Run {u"Linked text containers allow text which does not fit in one element to overflow into a different element on the page. Creative use of linked text containers enables basic multicolumn support and other advanced page layouts."},
    },
    Paragraph {
        Run {u"Duis sed nulla metus, id hendrerit velit. Curabitur dolor purus, bibendum eu cursus lacinia, interdum vel augue. Aenean euismod eros et sapien vehicula dictum. Duis ullamcorper, turpis nec feugiat tincidunt, dui erat luctus risus, aliquam accumsan lacus est vel quam. Nunc lacus massa, varius eget accumsan id, congue sed orci. Duis dignissim hendrerit egestas. Proin ut turpis magna, sit amet porta erat. Nunc semper metus nec magna imperdiet nec vestibulum dui fringilla. Sed sed ante libero, nec porttitor mi. Ut luctus, neque vitae placerat egestas, urna leo auctor magna, sit amet ultricies ipsum felis quis sapien. Proin eleifend varius dui, at vestibulum nunc consectetur nec. Mauris nulla elit, ultrices a sodales non, aliquam ac est. Quisque sit amet risus nulla. Quisque vestibulum posuere velit, vitae vestibulum eros scelerisque sit amet. In in risus est, at laoreet dolor. Nullam aliquet pellentesque convallis. Ut vel tincidunt nulla. Mauris auctor tincidunt auctor. Aenean orci ante, vulputate ac sagittis sit amet, consequat at mi. Morbi elementum purus consectetur nisi adipiscing vitae blandit sapien placerat. Aliquam adipiscing tortor non sem lobortis consectetur mattis felis rhoncus. Nunc eu nunc rhoncus arcu sollicitudin ultrices. In vulputate eros in mauris aliquam id dignissim nisl laoreet."},
    },
};

auto columns = Grid {
    height = 300,
    columnDefinitions = u"*,*,*",
    text,
    first,
    second,
};