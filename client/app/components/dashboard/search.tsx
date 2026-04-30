import React from "react";
import SearchIcon from "@mui/icons-material/Search";
import FormControl from "@mui/material/FormControl";
import OutlinedInput from '@mui/material/OutlinedInput';
import InputAdornment from '@mui/material/InputAdornment';

export default function Search() {
  return(
    <FormControl fullWidth sx={{ m: 1 }}>
      <OutlinedInput
        id="outlined-adornment-search"
        placeholder="Search"
        startAdornment={<InputAdornment position="start"><SearchIcon sx={{color: 'primary.main'}} /></InputAdornment>}
      />
    </FormControl>
  );
}