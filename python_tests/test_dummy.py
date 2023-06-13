import pytest
import caramel

@pytest.mark.unit
def test_dummy():
    dummy = caramel.Dummy()

    assert dummy.hello()