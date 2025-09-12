import os
import tempfile

import carameldb
import numpy as np
import pytest
from carameldb import BloomFilterConfig


def test_csf_get_filter():
    """Test that we can get the filter from a CSF."""
    keys = [f"key{i}" for i in range(100)]
    values = [i for i in range(100)]
    
    # Test CSF without filter
    csf_no_filter = carameldb.CSFUint32(keys, values)
    assert csf_no_filter.get_filter() is None
    
    # Test CSF with bloom filter - need to create data with high frequency element
    num_elements = 1000
    keys = [f"key{i}" for i in range(num_elements)]
    # Create data with 80% most common value
    num_most_common = int(num_elements * 0.8)
    other_elements = num_elements - num_most_common
    values = [10000 for _ in range(num_most_common)] + [i for i in range(other_elements)]
    
    csf_with_filter = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig())
    filter_obj = csf_with_filter.get_filter()
    
    # Filter should exist when we have a high frequency element
    assert filter_obj is not None
    assert hasattr(filter_obj, 'save')
    assert hasattr(filter_obj, 'get_most_common_value')
    assert filter_obj.get_most_common_value() == 10000


def test_csf_get_filter_not_created():
    """Test that get_filter returns None when bloom filter is not actually created."""
    # Case 1: All unique values - bloom filter won't be created
    keys = [f"key{i}" for i in range(100)]
    values = [f"value{i}" for i in range(100)]
    
    csf = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig())
    assert csf.get_filter() is None, "Filter should be None when all values are unique"
    
    # Case 2: Small dataset where frequency doesn't meet threshold
    keys = ["a", "b", "c", "d", "e"]
    values = ["x", "x", "y", "z", "w"]
    
    csf2 = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig())
    assert csf2.get_filter() is None, "Filter should be None when frequency threshold not met"
    
    # Case 3: All values are the same - bloom filter won't be created (bf_size = 0)
    keys = [f"key{i}" for i in range(50)]
    values = ["same"] * 50
    
    csf3 = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig())
    assert csf3.get_filter() is None, "Filter should be None when all values are identical"
    
    # Verify the CSFs still work correctly for queries
    assert csf.query("key0") == "value0"
    assert csf2.query("a") == "x"
    assert csf3.query("key10") == "same"


def test_filter_save_load():
    """Test saving and loading a filter independently."""
    # Create a CSF with a bloom filter
    num_elements = 1000
    keys = [f"key{i}" for i in range(num_elements)]
    # Create data with 80% most common value to trigger bloom filter
    num_most_common = int(num_elements * 0.8)
    other_elements = num_elements - num_most_common
    values = [10000 for _ in range(num_most_common)] + [i for i in range(other_elements)]
    
    csf = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig(error_rate=0.01))
    
    # Get the filter from the CSF
    filter_obj = csf.get_filter()
    
    if filter_obj is not None:
        # Save the filter
        with tempfile.NamedTemporaryFile(suffix='.filter', delete=False) as f:
            filter_filename = f.name
        
        try:
            filter_obj.save(filter_filename)
            
            # Load the filter back based on value type
            if isinstance(values[0], (int, np.integer)):
                if np.issubdtype(type(values[0]), np.uint64):
                    loaded_filter = carameldb.BloomPreFilterUint64.load(filter_filename)
                else:
                    loaded_filter = carameldb.BloomPreFilterUint32.load(filter_filename)
            elif isinstance(values[0], str):
                loaded_filter = carameldb.BloomPreFilterString.load(filter_filename)
            else:
                pytest.skip("Unsupported value type for filter test")
            
            # Verify the loaded filter works
            assert loaded_filter is not None
            
            # If it's a bloom filter, check we can get the bloom filter object
            if hasattr(loaded_filter, 'get_bloom_filter'):
                bloom = loaded_filter.get_bloom_filter()
                # Bloom filter might be None if all values are the same
                if bloom is not None:
                    # Test that the bloom filter has expected methods
                    assert hasattr(bloom, 'contains')
            
            # Check most common value is preserved
            if hasattr(loaded_filter, 'get_most_common_value'):
                most_common = loaded_filter.get_most_common_value()
                assert most_common == 10000
                
        finally:
            if os.path.exists(filter_filename):
                os.remove(filter_filename)


def test_csf_with_saved_filter():
    """Test that a CSF saved with a filter preserves the filter on load."""
    num_elements = 1000
    keys = [f"key{i}" for i in range(num_elements)]
    # Create data with 85% most common value to ensure bloom filter is created
    num_most_common = int(num_elements * 0.85)
    other_elements = num_elements - num_most_common
    values = [999 for _ in range(num_most_common)] + [i for i in range(other_elements)]
    
    # Create CSF with bloom filter
    csf = carameldb.Caramel(keys, values, prefilter=BloomFilterConfig(error_rate=0.05))
    
    # Save and load the CSF
    with tempfile.NamedTemporaryFile(suffix='.csf', delete=False) as f:
        csf_filename = f.name
    
    try:
        csf.save(csf_filename)
        loaded_csf = carameldb.load(csf_filename)
        
        # Check that the filter is preserved
        loaded_filter = loaded_csf.get_filter()
        original_filter = csf.get_filter()
        
        # Both should either be None or both should exist
        assert (loaded_filter is None) == (original_filter is None)
        
        # Verify queries still work correctly
        for key, value in zip(keys[:10], values[:10]):
            assert loaded_csf.query(key) == value
            
    finally:
        if os.path.exists(csf_filename):
            os.remove(csf_filename)


def test_filter_types():
    """Test different filter types for different value types."""
    test_cases = [
        ([1, 2, 3, 4, 5], np.uint32, carameldb.BloomPreFilterUint32),
        ([1, 2, 3, 4, 5], np.uint64, carameldb.BloomPreFilterUint64),
        (["hello", "world", "test"], str, carameldb.BloomPreFilterString),
    ]
    
    for values, dtype, filter_class in test_cases:
        num_elements = 100
        keys = [f"key{i}" for i in range(num_elements)]
        
        if dtype == str:
            # For strings, create with most common value
            test_values = ["common"] * 80 + values * 4
        else:
            # For numbers, create with most common value
            test_values = [999] * 80 + list(values) * 4
            test_values = np.array(test_values[:num_elements], dtype=dtype)
        
        csf = carameldb.Caramel(keys[:len(test_values)], test_values, 
                              prefilter=BloomFilterConfig(error_rate=0.01))
        
        filter_obj = csf.get_filter()
        if filter_obj is not None:
            # Test saving and loading with the correct type
            with tempfile.NamedTemporaryFile(suffix='.filter', delete=False) as f:
                filter_filename = f.name
            
            try:
                filter_obj.save(filter_filename)
                loaded_filter = filter_class.load(filter_filename)
                assert loaded_filter is not None
            finally:
                if os.path.exists(filter_filename):
                    os.remove(filter_filename)